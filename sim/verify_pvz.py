"""Check every submitted state's whole integer Game hash + telemetry."""
from pathlib import Path
import argparse
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('reference');parser.add_argument('rtl')
parser.add_argument('--allow-incomplete',action='store_true',help='short smoke run, no victory required')
parser.add_argument('--campaign',action='store_true',help='require victory in the three-lane second level')
args=parser.parse_args()
expected=Path(args.reference).read_text().splitlines()
actual=Path(args.rtl).read_text().splitlines()
if len(actual)<len(expected):raise SystemExit('FAIL: missing RTL frames')
for index,(a,b) in enumerate(zip(expected,actual)):
    if a!=b:raise SystemExit(f'FAIL frame {index+1}: reference={a}, RTL={b}')
if not args.allow_incomplete and not any(((int(row.split()[2],16)>>16)&15)==1 and
        (not args.campaign or (int(row.split()[2],16)&(1<<20))) for row in expected):
    raise SystemExit('FAIL: reference never completed the level')
print(f'PASS: {len(expected)} frames match reference Game hash/metrics'+(' (short smoke)' if args.allow_incomplete else '; level completed'))
