import os, re

root = r"D:\UnrealEngine-5.8.2\Engine\Source"
# CustomWidget 重载的特征：最后一个参数是 SNew(...) 或 TSharedRef<SWidget>
pat = re.compile(r'RegisterSettings\([^;]*?SNew\s*\(', re.S)

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
            out.append((p, t[m.start(): min(len(t), m.end() + 200)]))

print("用 CustomWidget 的注册:", len(out), "处\n")
for p, frag in out:
    print("=" * 72)
    print(p)
    print(frag[:500])
    print()
