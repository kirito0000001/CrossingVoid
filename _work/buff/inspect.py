from PIL import Image
import numpy as np

p = r"C:\CrossingVoid\_work\buff\atlas.png"
im = Image.open(p)
print("mode", im.mode, "size", im.size, "format", im.format)
print("info keys", list(im.info.keys()))
for k, v in im.info.items():
    s = str(v)
    print("  ", k, "=", s[:200])

a = np.asarray(im)
print("array shape", a.shape, a.dtype)
if a.ndim == 3 and a.shape[2] == 4:
    al = a[..., 3]
    vals, cnts = np.unique(al, return_counts=True)
    print("alpha unique count", len(vals))
    top = np.argsort(-cnts)[:10]
    for i in top:
        print(f"   alpha={vals[i]:>3}  {cnts[i]:>9}  {100*cnts[i]/al.size:5.2f}%")
    print("fully transparent px:", int((al == 0).sum()))
