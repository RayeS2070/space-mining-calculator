#!/usr/bin/env python3
"""Extracts space mining asteroid definitions from the GT5-Unofficial sources into JSON.

Reads every addRecipesToDrones(...) call in SpaceMiningRecipes.addAsteroids() and writes one
entry per asteroid with the values as passed to the call: the per drone tier expansion
(sizes, duration, EU/t) is left to the calculator, see README.md.

Only expression forms seen in the source are understood; anything else stops the script with
the offending expression, so a GT5U update can never be extracted silently wrong.

    tools/extract_asteroids.py [--gt5u ../gt5unofficial] [--out data/asteroids.json]
"""

import argparse
import json
import pathlib
import re
import subprocess
import sys

RECIPES = 'src/main/java/gtnhintergalactic/recipe/SpaceMiningRecipes.java'
DRONES = 'src/main/java/gtnhintergalactic/item/ItemMiningDrones.java'
VALUES = 'src/main/java/gregtech/api/enums/GTValues.java'
TIER_EU = 'src/main/java/gregtech/api/enums/TierEU.java'
BUILDER = 'src/main/java/gregtech/api/util/GTRecipeBuilder.java'

# Parameter order of addRecipesToDrones; both overloads share it, only `outputs` differs.
PARAMS = [
    'name', 'itemInputs', 'fluidInputs', 'chances', 'outputs', 'orePrefix', 'minSize', 'maxSize',
    'minDistance', 'maxDistance', 'computation', 'minModuleTier', 'duration', 'eut',
    'startDroneTier', 'endDroneTier', 'weight',
]


class ExtractError(Exception):
    pass


def call_args(text, start):
    """Splits the arguments of the call whose '(' is at `start`; returns (args, end)."""
    args, depth, cur, i = [], 0, [], start + 1
    while True:
        c = text[i]
        if c == '"':
            j = text.index('"', i + 1)
            cur.append(text[i:j + 1])
            i = j + 1
            continue
        if c in '([{':
            depth += 1
        elif c in ')]}':
            if depth == 0:
                args.append(''.join(cur))
                return [' '.join(a.split()) for a in args], i + 1
            depth -= 1
        elif c == ',' and depth == 0:
            args.append(''.join(cur))
            cur = []
            i += 1
            continue
        cur.append(c)
        i += 1


def enclosing_mod(text, pos):
    """Name of the mod in an enclosing `if (Mod.isModLoaded()) {`, or None."""
    depth = 0
    for i in range(pos - 1, -1, -1):
        c = text[i]
        if c == '}':
            depth += 1
        elif c == '{':
            if depth == 0:
                m = re.search(r'if \((\w+)\.isModLoaded\(\)\)\s*$', text[:i])
                if m:
                    return m.group(1)
            else:
                depth -= 1
    return None


def list_items(expr, kind):
    m = re.fullmatch(r'new ' + re.escape(kind) + r'\[\] \{ ?(.*?),? ?\}', expr)
    if not m:
        raise ExtractError(f'not a {kind}[] literal: {expr}')
    args, _ = call_args('(' + m.group(1) + ')', 0)
    return args


class Evaluator:
    """Turns the Java argument expressions into numbers and names."""

    def __init__(self, gt5u):
        values = (gt5u / VALUES).read_text()
        v = re.search(r'long\[\] V = new long\[\] \{([^}]*)\}', values).group(1)
        # The last entry (MAX) is an expression; recipes never use it, so it stays unknown.
        self.v = [int(x.rstrip('L').replace('_', '')) if re.fullmatch(r'[\d_]+L?', x) else None
                  for x in (y.strip() for y in v.split(',')) if x]
        if 'multiply(BigInteger.valueOf(30))' not in values or 'divide(BigInteger.valueOf(32))' not in values:
            raise ExtractError('GTValues.VP is no longer V * 30 / 32')
        tiers = re.findall(r'RECIPE_(\w+) = GTValues\.VP\[VoltageIndex\.(\w+)\]', (gt5u / TIER_EU).read_text())
        self.voltage_names = ['ULV', 'LV', 'MV', 'HV', 'EV', 'IV', 'LuV', 'ZPM', 'UV', 'UHV', 'UEV',
                              'UIV', 'UMV', 'UXV', 'MAX']
        self.tier_eu = {}
        for name, index in tiers:
            if name != index:
                raise ExtractError(f'TierEU.RECIPE_{name} is not VP[{name}]')
            v = self.v[self.voltage_names.index(index)]
            if v is not None:
                self.tier_eu[name] = v * 30 // 32
        seconds = re.search(r'int SECONDS = (\d+);', (gt5u / BUILDER).read_text())
        self.seconds = int(seconds.group(1))
        drones = (gt5u / DRONES).read_text()
        enum = re.search(r'enum DroneTiers \{(.*?);', drones, re.S).group(1)
        self.drone_tiers = re.findall(r'(\w+)\(ItemList\.MiningDrone\w+\)', enum)

    def int_(self, expr):
        if re.fullmatch(r'\d+', expr):
            return int(expr)
        m = re.fullmatch(r'(\d+) \* SECONDS', expr)
        if m:
            return int(m.group(1)) * self.seconds
        m = re.fullmatch(r'\(int\) TierEU\.RECIPE_(\w+)', expr)
        if m:
            return self.tier_eu[m.group(1)]
        raise ExtractError(f'unknown int expression: {expr}')

    def drone_tier(self, expr):
        m = re.fullmatch(r'ItemMiningDrones\.DroneTiers\.(\w+)\.ordinal\(\)', expr)
        if not m or m.group(1) not in self.drone_tiers:
            raise ExtractError(f'unknown drone tier: {expr}')
        return m.group(1)

    @staticmethod
    def material(expr):
        m = re.fullmatch(r'Materials\.(\w+)', expr)
        if m:
            return m.group(1)
        m = re.fullmatch(r'(?:WerkstoffLoader|GGMaterial)\.(\w+)\.getBridgeMaterial\(\)', expr)
        if m:
            return m.group(1)
        raise ExtractError(f'unknown material expression: {expr}')

    def item(self, expr):
        """An ItemStack expression -> output entry without chance."""
        m = re.fullmatch(r'GTOreDictUnificator\.get\(OrePrefixes\.(\w+), (.+), (\d+)\)', expr)
        if m:
            return {'name': self.material(m.group(2)), 'prefix': m.group(1), 'amount': int(m.group(3))}
        forms = [
            r'(?:MaterialsAlloy|MaterialsOres|MaterialMisc)\.(\w+)\.getOre\((\d+)\)',
            r'new ItemStack\(Blocks\.(\w+), (\d+)\)',
            r'GTModHandler\.getModItem\(\w+\.ID, "(\w+)", (\d+)(?:, \d+)?\)',
            r'(?:[\w.]+\.)?ItemList\.(\w+)\.get\((\d+)\)',
            r'ItemList\.(\w+)\.getWithCharge\((\d+), .+\)',
        ]
        for form in forms:
            m = re.fullmatch(form, expr)
            if m:
                return {'name': m.group(1), 'amount': int(m.group(2))}
        raise ExtractError(f'unknown item expression: {expr}')


def extract(gt5u):
    text = (gt5u / RECIPES).read_text()
    body_start = text.index('public static void addAsteroids()')
    body_end = text.index('public static ItemStack[] getTieredInputs')
    ev = Evaluator(gt5u)

    asteroids = []
    for m in re.finditer(r'addRecipesToDrones\(', text[:body_end]):
        if m.start() < body_start:
            continue
        args, _ = call_args(text, m.end() - 1)
        ore_overload = len(args) == len(PARAMS)
        if not ore_overload and len(args) != len(PARAMS) - 1:
            raise ExtractError(f'unexpected argument count {len(args)}: {args[:1]}')
        a = dict(zip(PARAMS if ore_overload else [p for p in PARAMS if p != 'orePrefix'], args))
        name = json.loads(a['name'])
        try:
            if a['itemInputs'] != 'null' or a['fluidInputs'] != 'null':
                raise ExtractError('extra item/fluid inputs are not supported')
            chances = [ev.int_(c) for c in list_items(a['chances'], 'int')]
            if ore_overload:
                prefix = re.fullmatch(r'OrePrefixes\.(\w+)', a['orePrefix'])
                if not prefix:
                    raise ExtractError(f'unknown ore prefix: {a["orePrefix"]}')
                # IG_RecipeAdder maps every ore to a stack of 64
                outputs = [{'name': ev.material(x), 'prefix': prefix.group(1), 'amount': 64}
                           for x in list_items(a['outputs'], 'Materials')]
            else:
                outputs = [ev.item(x) for x in list_items(a['outputs'], 'ItemStack')]
            if len(chances) < len(outputs):
                raise ExtractError('fewer chances than outputs: the recipe is never registered')
            for out, chance in zip(outputs, chances):
                out['chance'] = chance
            entry = {
                'name': name,
                'outputs': outputs,
                'minSize': ev.int_(a['minSize']),
                'maxSize': ev.int_(a['maxSize']),
                'minDistance': ev.int_(a['minDistance']),
                'maxDistance': ev.int_(a['maxDistance']),
                'computation': ev.int_(a['computation']),
                'minModuleTier': ev.int_(a['minModuleTier']),
                'duration': ev.int_(a['duration']),
                'eut': ev.int_(a['eut']),
                # Only the ore overload scales EU/t with the drone tier.
                'scaleEUt': ore_overload,
                'droneTiers': [ev.drone_tier(a['startDroneTier']), ev.drone_tier(a['endDroneTier'])],
                'weight': ev.int_(a['weight']),
            }
            mod = enclosing_mod(text, m.start())
            if mod:
                entry['requiresMod'] = mod
            asteroids.append(entry)
        except ExtractError as e:
            raise ExtractError(f'{name}: {e}') from None

    return {
        'source': {
            'file': RECIPES,
            'gt5u': git_describe(gt5u),
        },
        'droneTiers': ev.drone_tiers,
        'asteroids': asteroids,
    }


def git_describe(path):
    try:
        return subprocess.run(['git', '-C', str(path), 'describe', '--tags', '--always'],
                              capture_output=True, text=True, check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return 'unknown'


def main():
    here = pathlib.Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--gt5u', type=pathlib.Path, default=here.parent / 'gt5unofficial')
    parser.add_argument('--out', type=pathlib.Path, default=here / 'data' / 'asteroids.json')
    args = parser.parse_args()
    try:
        data = extract(args.gt5u)
    except ExtractError as e:
        sys.exit(f'extract_asteroids: {e}')
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(data, indent=2) + '\n')
    print(f'{len(data["asteroids"])} asteroids -> {args.out}')


if __name__ == '__main__':
    main()
