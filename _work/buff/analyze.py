import numpy as np
from PIL import Image

im = Image.open(r"C:\CrossingVoid\_work\buff\atlas.png").convert("RGBA")
a = np.asarray(im).astype(np.int16)
h, w = a.shape[:2]
print("size", w, h, "alpha min/max", a[..., 3].min(), a[..., 3].max())

rgb = a[..., :3]
# quantize to find dominant flat colors
q = (rgb // 4) * 4
flat = q.reshape(-1, 3)
uniq, cnt = np.unique(flat, axis=0, return_counts=True)
order = np.argsort(-cnt)
print("\n--- top 25 quantized colors ---")
for i in order[:25]:
    c = uniq[i]
    print(f"#{c[0]:02X}{c[1]:02X}{c[2]:02X}  rgb{tuple(int(v) for v in c)}  {cnt[i]:>9}  {100*cnt[i]/(w*h):5.2f}%")

print("\n--- exact corner / edge samples ---")
for name, (y, x) in {
    "topleft(2,2)": (2, 2),
    "top mid(2,1000)": (2, 1000),
    "row1 tile(150,300)": (150, 300),
    "page gap(330,60)": (330, 60),
    "navy box(300,560)": (300, 560),
    "bottom bar(1990,700)": (1990, 700),
}.items():
    print(name, a[y, x])
