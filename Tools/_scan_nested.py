import os, re

# 找：设置类里嵌套了别的设置对象的先例
root = r"D:\UnrealEngine-5.8.2\Engine\Source"
pat = re.compile(r'UPROPERTY\([^)]*Instanced[^)]*\)\s*\n\s*TObjectPtr<(\w+)>\s+(\w+)', re.M)

hits = []
for dp, dn, fn in os.walk(root):
    for f in fn:
        if not f.endswith(".h"):
            continue
        p = os.path.join(dp, f)
        try:
            t = open(p, encoding="utf-8", errors="ignore").read()
        except Exception:
            continue
        for m in pat.finditer(t):
            # 只看设置类
            head = t[: m.start()]
            if "UDeveloperSettings" not in head[-4000:]:
                continue
            hits.append((p, m.group(1), m.group(2)))

print("嵌套成员命中:", len(hits))
seen = set()
for p, ty, name in hits:
    key = (ty,)
    if key in seen:
        continue
    seen.add(key)
    print(f"  {ty} {name}   <- {os.path.basename(p)}")
