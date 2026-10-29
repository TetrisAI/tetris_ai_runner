#!/usr/bin/env python3
# Gate self-test for compare.py v3.
# Slices the real 9-case trace ONCE (small record-count per case, record-boundary complete),
# then derives negative fixtures. No C2_EXPECT_CASES env override exists anymore.
# Every negative must exit non-zero; positives must exit 0.
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CMP = os.path.join(HERE, "compare.py")
OLD = os.path.join(HERE, "out_old3.txt")
NEW = os.path.join(HERE, "out_final4.txt")
BROKEN = os.path.join(HERE, "out_broken.txt")
FX = os.path.join(HERE, "gate_selftest")
K = 3   # records per case in the small fixture


def slice_9_cases(path, k):
    """Return list of 9 case blocks; each block = (case_line, [record blocks])."""
    cases = []
    cur_case = None
    cur_rec = []
    recs = []
    def close_case():
        nonlocal cur_case, cur_rec, recs
        if cur_case is None:
            return
        if cur_rec:
            recs.append(cur_rec); cur_rec = []
        cases.append((cur_case, recs)); recs = []
    done = False
    with open(path) as f:
        for line in f:
            t = line.split("\t")
            if t[0] == "CASE":
                close_case()
                if len(cases) >= 9:
                    break
                cur_case = line
                done = False
            elif done:
                continue
            elif t[0] == "G":
                if cur_rec:
                    recs.append(cur_rec)
                    if len(recs) >= k:
                        done = True
                        cur_rec = []
                        continue
                cur_rec = [line]
            else:
                cur_rec.append(line)
        close_case()
    return cases


def render(cases):
    out = []
    for cl, recs in cases:
        out.append(cl)
        for r in recs:
            out.extend(r)
    return out


def write(name, lines):
    p = os.path.join(FX, name)
    with open(p, "w") as f:
        f.writelines(lines)
    return p


def run(old, new, expect, note):
    r = subprocess.run([sys.executable, CMP, old, new], capture_output=True, text=True)
    ok = (r.returncode == 0) == expect
    print(f"[{'PASS' if ok else 'FAIL'}] rc={r.returncode} expect_rc={'0' if expect else 'nonzero'}  {note}")
    for ln in (r.stdout + r.stderr).strip().splitlines()[-2:]:
        print(f"      | {ln}")
    return r.returncode, ok


def main():
    os.makedirs(FX, exist_ok=True)
    old_cases = slice_9_cases(OLD, K)
    new_cases = slice_9_cases(NEW, K)
    assert len(old_cases) == 9 and len(new_cases) == 9, f"slice got {len(old_cases)}/{len(new_cases)}"

    codes = {}
    p_old = write("fx_old.txt", render(old_cases))
    p_new = write("fx_new.txt", render(new_cases))

    # 1. POSITIVE small fixture (9 frozen names, 3 rec each) -> rc 0
    codes["pos_small"], ok = run(p_old, p_new, True, "pos small 9-case fixture old==new")
    if not ok:
        print("!! positive small fixture failed; aborting"); return 1

    # 2. extra case on new side -> frozen names gate
    extra = render(new_cases) + ["CASE\textra_case\n"] + new_cases[0][1][0]
    codes["extra_case"], _ = run(p_old, write("fx_new_extracase.txt", extra), False, "extra 10th case on new side")

    # 3. missing case on new side (drop clear3 block) -> frozen names gate
    codes["missing_case"], _ = run(p_old, write("fx_new_missingcase.txt",
        render([c for i, c in enumerate(new_cases) if i != 6])), False, "missing clear3 case on new side")

    # 4. empty input variants
    p_empty = write("fx_empty.txt", [])
    codes["empty_input_new"], _ = run(p_old, p_empty, False, "empty new input")
    codes["empty_input_old"], _ = run(p_empty, p_new, False, "empty old input")
    codes["empty_both"], _ = run(p_empty, p_empty, False, "empty both sides")

    # 5. extra / missing G record -> per-case record-count gate
    extra_rec = render(new_cases) + new_cases[0][1][0]
    codes["extra_record"], _ = run(p_old, write("fx_new_extrarec.txt", extra_rec), False, "extra G record on new side")
    lean_cases = [(cl, recs[1:]) if i == 0 else (cl, recs) for i, (cl, recs) in enumerate(old_cases)]
    codes["missing_record"], _ = run(write("fx_old_missingrec.txt", render(lean_cases)), p_new, False, "missing G record on old side")

    # 6. same-total-count multiset skew: drop rec1 of case0, duplicate rec0 -> unmatched branch
    skew_cases = [(cl, recs) if i != 0 else (cl, [recs[2], recs[0], recs[0]])
                  for i, (cl, recs) in enumerate(new_cases)]
    codes["multiset_same_len"], _ = run(p_old, write("fx_new_skew.txt", render(skew_cases)),
                                        False, "same count, skewed chain multiset (unmatched branch)")

    # 7. field diff (nn) on case0 rec1
    mut = []
    for i, (cl, recs) in enumerate(new_cases):
        mut.append(cl)
        for j, r in enumerate(recs):
            blk = list(r)
            if i == 0 and j == 1:
                g = blk[0].rstrip("\n").split("\t"); g[7] = "X"; blk[0] = "\t".join(g) + "\n"
            mut.extend(blk)
    codes["field_diff"], _ = run(p_old, write("fx_new_fielddiff.txt", mut), False, "field diff (nn) case0 rec1")

    # 8. missing XD / MA / MB sections
    def strip(tag, case_i, rec_j):
        out = []
        for i, (cl, recs) in enumerate(new_cases):
            out.append(cl)
            for j, r in enumerate(recs):
                out.extend([l for l in r if not (i == case_i and j == rec_j and l.startswith(tag + "\t"))])
        return out
    codes["missing_xd"], _ = run(p_old, write("fx_new_noxd.txt", strip("XD", 0, 1)), False, "record missing XD section")
    codes["missing_ma"], _ = run(p_old, write("fx_new_noma.txt", strip("MA", 0, 1)), False, "record missing MA section")
    codes["missing_mb"], _ = run(p_old, write("fx_new_nomb.txt", strip("MB", 0, 1)), False, "record missing MB section")

    # 9. missing G line of case0 rec1 -> stray XD/MA tag collision
    nog = []
    for i, (cl, recs) in enumerate(new_cases):
        nog.append(cl)
        for j, r in enumerate(recs):
            blk = list(r)
            if i == 0 and j == 1:
                blk = blk[1:]
            nog.extend(blk)
    codes["missing_g"], _ = run(p_old, write("fx_new_nog.txt", nog), False, "record missing its G line (stray sections)")

    # 10. BOTH sides delete the same MA row (y5) of case0 rec1 -> structural gate must still fatal
    def strip_row(lines, ytag):
        return [l for l in lines if not (l.startswith("MA\t") and f"\ty{ytag}=" in l)]
    both_old = strip_row(render(old_cases), 5)
    both_new = strip_row(render(new_cases), 5)
    codes["both_sides_same_ma_row"], _ = run(write("fx_old_samrow.txt", both_old),
                                            write("fx_new_samrow.txt", both_new),
                                            False, "BOTH sides delete same MA row (must not green)")

    # 11. empty case block: case3 (neither) keeps its CASE line but zero records
    ec_cases = [(cl, [] if i == 3 else recs) for i, (cl, recs) in enumerate(new_cases)]
    codes["empty_case_block"], _ = run(p_old, write("fx_new_emptycase.txt", render(ec_cases)),
                                       False, "one case block has 0 G records")

    # 12. BOTH sides rename all cases to the SAME wrong list -> frozen-names gate fatal
    def rename(lines):
        return [l.replace("virt_", "virtX_") if l.startswith("CASE\t") else l for l in lines]
    codes["both_same_wrong_names"], _ = run(write("fx_old_ren.txt", rename(render(old_cases))),
                                            write("fx_new_ren.txt", rename(render(new_cases))),
                                            False, "BOTH sides renamed identically (frozen-names gate)")

    # 13. dup-chain ordinal pairing: craft two identical chains in case0 on both sides
    b0 = new_cases[0][1][0]
    def withev(block, ev):
        blk = list(block)
        g = blk[0].rstrip("\n").split("\t"); g[10] = ev; blk[0] = "\t".join(g) + "\n"
        return blk
    def dup_pair(ev1, ev2):
        out = [new_cases[0][0]] + withev(b0, ev1) + withev(b0, ev2)
        for i in range(1, 9):
            out.append(new_cases[i][0])
            for r in new_cases[i][1]:
                out.extend(r)
        return out
    codes["dup_ordered"], ok1 = run(write("fx_dup_o.txt", dup_pair("ev=AAAA", "ev=BBBB")),
                                    write("fx_dup_n.txt", dup_pair("ev=AAAA", "ev=BBBB")),
                                    True, "dup chain x2 same ordinal order")
    codes["dup_shuffled"], _ = run(write("fx_dup_o2.txt", dup_pair("ev=AAAA", "ev=BBBB")),
                                   write("fx_dup_n2.txt", dup_pair("ev=BBBB", "ev=AAAA")),
                                   False, "dup chain x2 shuffled occurrences (ordinal mismatch fatal)")

    # 14. REAL full traces
    r = subprocess.run([sys.executable, CMP, OLD, NEW], capture_output=True, text=True)
    codes["real_old3_vs_final4"] = r.returncode
    print(f"\n[{'PASS' if r.returncode == 0 else 'FAIL'}] REAL out_old3 vs out_final4 rc={r.returncode} (expect 0)")
    for ln in (r.stdout + r.stderr).strip().splitlines()[-5:]:
        print(f"      | {ln}")

    r2 = subprocess.run([sys.executable, CMP, BROKEN, OLD], capture_output=True, text=True)
    codes["real_broken_vs_old3"] = r2.returncode
    print(f"[{'PASS' if r2.returncode != 0 else 'FAIL'}] REAL out_broken vs out_old3 rc={r2.returncode} (expect nonzero)")
    for ln in (r2.stdout + r2.stderr).strip().splitlines()[-3:]:
        print(f"      | {ln}")

    print("\n=== SUMMARY rc map ===")
    for k, v in codes.items():
        print(f"  {k}: rc={v}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
