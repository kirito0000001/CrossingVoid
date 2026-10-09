from PIL import Image
import numpy as np

src = Image.open(r"C:\CrossingVoid\_work\buff\atlas.png").convert("RGBA")
a = np.asarray(src).astype(np.float32)
h, w = a.shape[:2]
rgb, al = a[..., :3], a[..., 3:4] / 255.0

out = r"C:\CrossingVoid\_work\buff"
# composite over white
white = np.full_like(rgb, 255.0)
comp_w = (rgb * al + white * (1 - al)).astype(np.uint8)
Image.fromarray(comp_w).resize((1024, 1024), Image.LANCZOS).save(out + r"\view_over_white.png")

# composite over checkerboard (magenta / dark) to expose transparency
yy, xx = np.mgrid[0:h, 0:w]
check = np.where(((yy // 64) + (xx // 64)) % 2 == 0, 255.0, 60.0)[..., None]
comp_c = (rgb * al + check * (1 - al)).astype(np.uint8)
Image.fromarray(comp_c).resize((1024, 1024), Image.LANCZOS).save(out + r"\view_over_checker.png")

# alpha channel alone
Image.fromarray(a[..., 3].astype(np.uint8)).resize((1024, 1024), Image.NEAREST).save(out + r"\view_alpha.png")

# rgb only (ignore alpha) - what the raw colors look like
Image.fromarray(a[..., :3].astype(np.uint8)).resize((1024, 1024), Image.LANCZOS).save(out + r"\view_rgb_ignoring_alpha.png")
print("ok")
print("corner rgb/alpha:", a[0, 0], a[0, 1000], a[1000, 0], a[2047, 2047])
print("center rgb/alpha:", a[1024, 1024])
