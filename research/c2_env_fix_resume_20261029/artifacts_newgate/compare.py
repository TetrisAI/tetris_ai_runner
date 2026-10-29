#!/usr/bin/env python3
# C2 strict trace comparator v3 — hard gate, frozen expected format.
#
# Trace stream shape (emitted by test_c2_trace_driver.cpp observer), per record:
#   G  vp chain tp clr dep par nn nl hd ev rout        (12 tab cells)
#   XD <chain> new_hole=<..> sdrop=<..> hcount_raw=<..> HoleCountSrc=<..>   (exactly 1, 6 cells)
#   MA <chain> y0=.. ... y27=..                         (exactly 28 rows, y0..y27 in order)
#   MB <chain> y0=.. ... y27=..                         (exactly 28 rows, y0..y27 in order)
#
# Parser attaches XD/MA/MB to the record they FOLLOW (stream order) — NOT grouped by
# chain name: duplicate chain keys keep per-occurrence sections; a later occurrence's
# XD must never overwrite an earlier occurrence's section.
#
# Frozen gates (any violation => non-zero exit; records are never filtered to force green):
#   1. both inputs non-empty; every parsed case block carries >=1 G record.
#   2. the case name list is EXACTLY the frozen expected 9 cases, ordered (hard-coded;
#      no env override — both sides being renamed to the same wrong list cannot green).
#   3. per case: G record count identical on both sides (zip() may not truncate).
#   4. every record structurally complete AND in the exact emitted format: 1 XD with the
#      fixed 4 payload keys in order; MA list exactly 28 rows y0=.. .. y27=..; MB list
#      the same; chain tags on XD/MA/MB equal the record's chain. An incomplete record
#      can never silently match (no None == None).
#   5. per case chain multiset: any-side unmatched chain occurrence > 0 => fatal.
#   6. per unique chain, occurrences paired by ordinal exactly once (duplicate
#      multiplicity preserved; oc==nc already guarantees equal occurrence lists).
#   7. paired records must match on all 10 G fields + MA list + MB list + XD list.
#
# Exit codes: 0 gate passed; 2 field mismatch on a paired record;
# 3 structural/gate violation; 1 usage error.
import sys
from collections import Counter, defaultdict

EXPECTED_CASE_NAMES = [
    "virt_preview_T_combo3",
    "virt_only_T_combo3",
    "preview_only_T_combo3",
    "neither_T_combo3",
    "clear1_I_combo3",
    "clear4_I_combo3",
    "clear3_I_combo3",
    "clear2_I_combo3",
    "virt_preview_T_combo0",
]
MA_ROWS = [f"y{i}=" for i in range(28)]
XD_KEYS = ("new_hole=", "sdrop=", "hcount_raw=", "HoleCountSrc=")
FIELDS = ["vp", "tp", "clr", "dep", "par", "nn", "nl", "hd", "ev", "rout"]


class GateFail(Exception):
    pass


def parse(path):
    """Stream-parse one trace into [(case_name, chain -> [records in order])].

    Raises GateFail on malformed lines or records that are not structurally complete in
    the exact emitted format.
    """
    cases = []
    name = None
    recs = defaultdict(list)
    cur = None

    def flush_case():
        nonlocal name, recs, cur
        if name is None:
            return
        if cur is not None:
            recs[cur["chain"]].append(cur)
            cur = None
        for lst in recs.values():
            for r in lst:
                check_complete(path, r)
        cases.append((name, dict(recs)))
        recs = defaultdict(list)

    with open(path) as f:
        for lineno, line in enumerate(f, 1):
            t = line.rstrip("\n").split("\t")
            tag = t[0]
            if tag == "CASE":
                flush_case()
                if len(t) < 2:
                    raise GateFail(f"{path}:{lineno}: CASE line missing case name")
                name = t[1]
            elif tag == "G":
                if name is None:
                    raise GateFail(f"{path}:{lineno}: G record outside any CASE")
                if cur is not None:
                    recs[cur["chain"]].append(cur)
                    cur = None
                if len(t) != 12:
                    raise GateFail(
                        f"{path}:{lineno}: G record has {len(t)} tab-fields, want 12")
                cur = {"vp": t[1], "chain": t[2], "tp": t[3], "clr": t[4], "dep": t[5],
                       "par": t[6], "nn": t[7], "nl": t[8], "hd": t[9], "ev": t[10],
                       "rout": t[11], "xd": [], "ma": [], "mb": []}
            elif tag == "XD":
                if cur is None:
                    raise GateFail(f"{path}:{lineno}: XD line outside any G record")
                if t[1] != cur["chain"]:
                    raise GateFail(
                        f"{path}:{lineno}: XD chain {t[1]!r} != record chain {cur['chain']!r}")
                cur["xd"].append(t[1:])   # [chain, new_hole=.., sdrop=.., hcount_raw=.., HoleCountSrc=..]
            elif tag == "MA":
                if cur is None:
                    raise GateFail(f"{path}:{lineno}: MA line outside any G record")
                if t[1] != cur["chain"]:
                    raise GateFail(
                        f"{path}:{lineno}: MA chain {t[1]!r} != record chain {cur['chain']!r}")
                cur["ma"].append(t[2] if len(t) > 2 else "")
            elif tag == "MB":
                if cur is None:
                    raise GateFail(f"{path}:{lineno}: MB line outside any G record")
                if t[1] != cur["chain"]:
                    raise GateFail(
                        f"{path}:{lineno}: MB chain {t[1]!r} != record chain {cur['chain']!r}")
                cur["mb"].append(t[2] if len(t) > 2 else "")
            else:
                raise GateFail(f"{path}:{lineno}: unknown tag {tag!r}")
        flush_case()
    return cases


def _row_index_bad(rows, kind):
    for i, p in enumerate(rows):
        if not p.startswith(MA_ROWS[i]):
            return f"{kind} row[{i}]={p!r} does not start with {MA_ROWS[i]!r}"
    return None


def check_complete(path, r):
    """A record missing any section, or in the wrong shape, can never silently match."""
    why = None
    if len(r["xd"]) != 1:
        why = f"xd sections={len(r['xd'])} (want exactly 1)"
    else:
        payload = r["xd"][0]          # [chain, k1, k2, k3, k4]
        if len(payload) != 5:
            why = f"xd payload cells={len(payload)} (want 5: chain + 4 keys)"
        else:
            for key, cell in zip(XD_KEYS, payload[1:]):
                if not cell.startswith(key):
                    why = f"xd payload order: {cell!r} != expected {key!r}"; break
    if why is None and len(r["ma"]) != 28:
        why = f"MA rows={len(r['ma'])} (want 28, y0..y27)"
    if why is None:
        bad = _row_index_bad(r["ma"], "MA")
        if bad:
            why = bad
    if why is None and len(r["mb"]) != 28:
        why = f"MB rows={len(r['mb'])} (want 28, y0..y27)"
    if why is None:
        bad = _row_index_bad(r["mb"], "MB")
        if bad:
            why = bad
    if why:
        raise GateFail(
            f"{path}: record chain={r['chain']!r} vp={r['vp']!r} structurally incomplete ({why})")


def gcount(case):
    return sum(len(v) for v in case.values())


def main():
    if len(sys.argv) != 3:
        print("usage: compare.py old_trace.txt new_trace.txt", file=sys.stderr)
        return 1
    oldp, newp = sys.argv[1], sys.argv[2]
    try:
        old = parse(oldp)
        new = parse(newp)
    except GateFail as e:
        print(f"# GATE structural failure: {e}", file=sys.stderr)
        return 3

    # Gate 1: non-empty inputs; every case block non-empty
    if not old:
        print(f"# GATE failure: {oldp} parses to 0 cases", file=sys.stderr)
        return 3
    if not new:
        print(f"# GATE failure: {newp} parses to 0 cases", file=sys.stderr)
        return 3
    for label, side in (("old", old), ("new", new)):
        for nm, recs in side:
            if gcount(recs) == 0:
                print(f"# GATE failure: {label} case {nm!r} has 0 G records", file=sys.stderr)
                return 3

    # Gate 2: frozen expected case names, ordered
    old_names = [n for n, _ in old]
    new_names = [n for n, _ in new]
    if old_names != EXPECTED_CASE_NAMES:
        print(f"# GATE failure: old case names != frozen expected list\n"
              f"#   got={old_names}", file=sys.stderr)
        return 3
    if new_names != EXPECTED_CASE_NAMES:
        print(f"# GATE failure: new case names != frozen expected list\n"
              f"#   got={new_names}", file=sys.stderr)
        return 3

    old_g = sum(gcount(c) for _, c in old)
    new_g = sum(gcount(c) for _, c in new)
    print(f"# cases=9 frozen; oldG={old_g} newG={new_g}")

    tm = tu = tn = 0
    old_dup_extras = new_dup_extras = 0
    uniq_old = uniq_new = 0
    routs = set()
    first_fail = None
    for (onm, orec), (nnm, nrec) in zip(old, new):
        ol = sum(len(v) for v in orec.values())
        nl = sum(len(v) for v in nrec.values())
        # Gate 3: per-case record length identical
        if ol != nl:
            print(f"# GATE failure: case {onm!r} record count old={ol} new={nl}",
                  file=sys.stderr)
            return 3
        oc = Counter({c: len(v) for c, v in orec.items()})
        nc = Counter({c: len(v) for c, v in nrec.items()})
        old_dup_extras += sum(v - 1 for v in oc.values() if v > 1)
        new_dup_extras += sum(v - 1 for v in nc.values() if v > 1)
        uniq_old += len(orec)
        uniq_new += len(nrec)
        only_old = sorted((oc - nc).elements())
        only_new = sorted((nc - oc).elements())
        matched = list((oc & nc).elements())
        tm += len(matched)
        tu += len(only_old)
        tn += len(only_new)
        print(f"\n== {onm}: oldG={ol} newG={nl} matched={len(matched)} "
              f"only_old={len(only_old)} only_new={len(only_new)}")
        # Gate 5: any-side unmatched => fatal
        if only_old or only_new:
            print(f"# GATE failure: case {onm!r} unmatched-old={len(only_old)} "
                  f"unmatched-new={len(only_new)}", file=sys.stderr)
            print(f"#   sample only_old={only_old[:5]} only_new={only_new[:5]}",
                  file=sys.stderr)
            return 3
        # Gate 6+7: each unique chain paired by ordinal exactly once
        for c in sorted(orec.keys() & nrec.keys()):
            oseq = orec[c]
            nseq = nrec[c]
            for i, (o, n) in enumerate(zip(oseq, nseq)):
                for fld in FIELDS:
                    if o[fld] != n[fld]:
                        if first_fail is None:
                            first_fail = (onm, c, i, fld, o[fld], n[fld])
                if o["ma"] != n["ma"] and first_fail is None:
                    first_fail = (onm, c, i, "MA", o["ma"], n["ma"])
                if o["mb"] != n["mb"] and first_fail is None:
                    first_fail = (onm, c, i, "MB", o["mb"], n["mb"])
                if o["xd"] != n["xd"] and first_fail is None:
                    first_fail = (onm, c, i, "XD", o["xd"], n["xd"])
        for lst in orec.values():
            for r in lst:
                routs.add(r["rout"])

    print(f"\n# TOTAL matched_records={tm} unmatched_old={tu} unmatched_new={tn}")
    print(f"# per-side dup extras: old={old_dup_extras} new={new_dup_extras}")
    print(f"# unique_chain_keys: old={uniq_old} new={uniq_new} route_distinct={len(routs)}")
    if first_fail:
        cse, chain, ordi, fld, ov, nv = first_fail
        print(f"# FIRST_MISMATCH case={cse} chain={chain} occ[{ordi}] field={fld}")
        print(f"#   OLD={ov}")
        print(f"#   NEW={nv}")
        return 2
    if tu or tn:
        print(f"# GATE failure: leftovers unmatched_old={tu} unmatched_new={tn}",
              file=sys.stderr)
        return 3
    print("# GATE PASS: every paired record matches on G fields + MA + MB + XD; no unmatched")
    return 0


if __name__ == "__main__":
    sys.exit(main())
