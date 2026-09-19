import os, re, sys

root = r"D:\UnrealEngine-5.8.2\Engine\Source"
pat = re.compile(r'RegisterSettings\(\s*\n?\s*"Project"')

out = []
for dp, dn, fn in os.walk(root):
    for f in fn:
        if not f.endswith(".cpp"):
            continue
        p = os.path.join(dp, f)
        try:
            t = open(p, encoding="utf-8", errors="ignore").read()
        except Exception:
            continue
        for m in pat.finditer(t):
            s = max(0, m.start() - 200)
            e = min(len(t), m.end() + 600)
            out.append((p, t[s:e]))

print("命中", len(out), "处\n")
for p, frag in out[:12]:
    print("=" * 72)
    print(p)
    print(frag)
    print()
