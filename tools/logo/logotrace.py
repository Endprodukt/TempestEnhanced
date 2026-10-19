"""logotrace.py - the marquee's TEMPEST lettering as vector lines.

    python tools/logo/logotrace.py [tools/logo/tempest_logo.png] [src/render/logo_shape.h]

Writes the C header render/logo.c draws the title with (in place of the
game's own logo, see render/logo.h). The picture is split into masks with
ImageMagick (magick on the PATH):

  face   the letters' fronts (red, orange, yellow): traced as outlines,
         each in the average colour of its letter
  edge   the cream sides the letters stand out with: thinned to their
         middle lines

Outlines are traced along the pixels' edges and simplified (Douglas-Peucker);
the lines then lie in the game's generator units, where its own logo lies
when it has come all the way in: x -195..203 from the top -146 down (as
tall as the picture's proportions make it).
"""
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
SRC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, 'tempest_logo.png')
OUT = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, 'src', 'render', 'logo_shape.h')

X0, X1, TOP = -195.0, 203.0, -146.0     # where the game's logo lies, come in
FACE_EPS = 1.1                          # simplification, pixels
EDGE_EPS = 1.2
MIN_AREA = 40                           # outlines smaller than this (pixels) are noise
MIN_EDGE = 10                           # middle lines shorter than this too

FACE_FX = '(u.a>0.5 && r>0.47 && b<0.35 && r-b>0.4 && !(g>0.6 && b>0.4)) ? 1 : 0'
EDGE_FX = '(u.a>0.5 && r>0.55 && g>0.55 && b>0.35) ? 1 : 0'
SOLID_FX = '(u.a>0.5 && !(b>r+0.12)) ? 1 : 0'


def magick(*args):
    subprocess.run(['magick'] + list(args), check=True)


def read_pnm(path):
    """A plain (P2 or P3) picture: width, height, values (a list per pixel)."""
    tok = []
    with open(path) as f:
        for line in f:
            line = line.split('#')[0]
            tok.extend(line.split())
    kind, w, h, mx = tok[0], int(tok[1]), int(tok[2]), int(tok[3])
    ch = 3 if kind == 'P3' else 1
    v = [int(t) * 255 // mx for t in tok[4:4 + w * h * ch]]
    if ch == 1:
        return w, h, v
    return w, h, [tuple(v[i:i + 3]) for i in range(0, len(v), 3)]


def mask(src, fx, tmp, name, morph=None):
    out = os.path.join(tmp, name + '.pgm')
    args = [src, '-channel', 'RGBA', '-fx', fx, '-alpha', 'off', '-colorspace', 'gray']
    if morph:
        args += morph
    magick(*(args + ['-threshold', '50%', '-compress', 'none', out]))
    w, h, v = read_pnm(out)
    return w, h, [1 if p > 127 else 0 for p in v]


# --- outlines ---------------------------------------------------------------

def outlines(w, h, m):
    """Closed loops along the pixels' edges, the foreground on the left."""
    def fg(x, y):
        return 0 <= x < w and 0 <= y < h and m[y * w + x]
    nxt = {}
    for y in range(h):
        for x in range(w):
            if not m[y * w + x]:
                continue
            if not fg(x, y - 1): nxt.setdefault((x + 1, y), []).append((x, y))
            if not fg(x - 1, y): nxt.setdefault((x, y), []).append((x, y + 1))
            if not fg(x, y + 1): nxt.setdefault((x, y + 1), []).append((x + 1, y + 1))
            if not fg(x + 1, y): nxt.setdefault((x + 1, y + 1), []).append((x + 1, y))
    loops = []
    while nxt:
        start = next(iter(nxt))
        loop = [start]
        cur, prev = start, None
        while True:
            outs = nxt.get(cur)
            if not outs:
                break
            if len(outs) == 1 or prev is None:
                n = outs[0]
            else:
                # two ways on (pixels touching at a corner): the sharpest turn
                # to the right keeps them apart
                dx, dy = cur[0] - prev[0], cur[1] - prev[1]
                def turn(p):
                    ex, ey = p[0] - cur[0], p[1] - cur[1]
                    return dx * ey - dy * ex
                n = min(outs, key=turn)
            outs.remove(n)
            if not outs:
                del nxt[cur]
            prev, cur = cur, n
            if cur == start:
                break
            loop.append(cur)
        if len(loop) > 2:
            loops.append(loop)
    return loops


def area(loop):
    a = 0.0
    for i in range(len(loop)):
        x0, y0 = loop[i - 1]
        x1, y1 = loop[i]
        a += x0 * y1 - x1 * y0
    return a * 0.5


def dp(pts, eps):
    """Douglas-Peucker on an open polyline."""
    if len(pts) < 3:
        return list(pts)
    keep = [False] * len(pts)
    keep[0] = keep[-1] = True
    stack = [(0, len(pts) - 1)]
    while stack:
        a, b = stack.pop()
        ax, ay = pts[a]
        bx, by = pts[b]
        dx, dy = bx - ax, by - ay
        ln = (dx * dx + dy * dy) ** 0.5
        best, bi = -1.0, -1
        for i in range(a + 1, b):
            px, py = pts[i]
            if ln < 1e-9:
                d = ((px - ax) ** 2 + (py - ay) ** 2) ** 0.5
            else:
                d = abs(dx * (ay - py) - dy * (ax - px)) / ln
            if d > best:
                best, bi = d, i
        if best > eps:
            keep[bi] = True
            stack.append((a, bi))
            stack.append((bi, b))
    return [p for p, k in zip(pts, keep) if k]


def simplify_loop(loop, eps):
    # the edges' middles (half the staircase), then from the point farthest
    # from the first both halves on their own
    mid = [((loop[i - 1][0] + loop[i][0]) * 0.5, (loop[i - 1][1] + loop[i][1]) * 0.5) for i in range(len(loop))]
    far = max(range(len(mid)), key=lambda i: (mid[i][0] - mid[0][0]) ** 2 + (mid[i][1] - mid[0][1]) ** 2)
    a = dp(mid[:far + 1], eps)
    b = dp(mid[far:] + [mid[0]], eps)
    return a[:-1] + b[:-1]


# --- middle lines -----------------------------------------------------------

def thin(w, h, m):
    """Zhang-Suen thinning."""
    m = list(m)
    def n8(x, y):
        def g(xx, yy):
            return m[yy * w + xx] if 0 <= xx < w and 0 <= yy < h else 0
        return [g(x, y - 1), g(x + 1, y - 1), g(x + 1, y), g(x + 1, y + 1),
                g(x, y + 1), g(x - 1, y + 1), g(x - 1, y), g(x - 1, y - 1)]
    changed = True
    while changed:
        changed = False
        for step in (0, 1):
            gone = []
            for y in range(h):
                for x in range(w):
                    if not m[y * w + x]:
                        continue
                    p = n8(x, y)
                    b = sum(p)
                    if b < 2 or b > 6:
                        continue
                    a = sum(1 for i in range(8) if p[i] == 0 and p[(i + 1) % 8] == 1)
                    if a != 1:
                        continue
                    if step == 0 and (p[0] * p[2] * p[4] or p[2] * p[4] * p[6]):
                        continue
                    if step == 1 and (p[0] * p[2] * p[6] or p[0] * p[4] * p[6]):
                        continue
                    gone.append(y * w + x)
            for i in gone:
                m[i] = 0
            changed = changed or bool(gone)
    return m


def chains(w, h, m):
    """The thinned lines as polylines, from end to end (or junction)."""
    on = {(i % w, i // w) for i in range(w * h) if m[i]}
    def nb(p):
        x, y = p
        return [(x + dx, y + dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1)
                if (dx or dy) and (x + dx, y + dy) in on]
    deg = {p: len(nb(p)) for p in on}
    used = set()
    out = []
    starts = [p for p in on if deg[p] != 2] + [p for p in on if deg[p] == 2]
    for s in starts:
        for q in nb(s):
            e = frozenset((s, q))
            if e in used:
                continue
            line = [s, q]
            used.add(e)
            prev, cur = s, q
            while deg[cur] == 2:
                nx = [r for r in nb(cur) if r != prev and frozenset((cur, r)) not in used]
                if not nx:
                    break
                used.add(frozenset((cur, nx[0])))
                prev, cur = cur, nx[0]
                line.append(cur)
                if cur == s:
                    break
            out.append(line)
    return out


def length(pts):
    return sum(((pts[i][0] - pts[i - 1][0]) ** 2 + (pts[i][1] - pts[i - 1][1]) ** 2) ** 0.5 for i in range(1, len(pts)))


# --- the header -------------------------------------------------------------

def main():
    tmp = tempfile.mkdtemp()
    w, h, face = mask(SRC, FACE_FX, tmp, 'face', ['-morphology', 'Open', 'Disk:1', '-morphology', 'Close', 'Disk:1'])
    _, _, edge = mask(SRC, EDGE_FX, tmp, 'edge')
    _, _, solid = mask(SRC, SOLID_FX, tmp, 'solid')
    rgb_path = os.path.join(tmp, 'rgb.ppm')
    magick(SRC, '-alpha', 'off', '-compress', 'none', rgb_path)
    _, _, rgb = read_pnm(rgb_path)

    # the lettering's box: what is not the purple border
    xs = [i % w for i in range(w * h) if solid[i]]
    ys = [i // w for i in range(w * h) if solid[i]]
    bx0, bx1, by0 = min(xs), max(xs) + 1, min(ys)
    s = (X1 - X0) / (bx1 - bx0)

    def gen(p):
        return (X0 + (p[0] - bx0) * s, TOP + (p[1] - by0) * s)

    paths = []      # (points, closed, colour)
    for loop in outlines(w, h, face):
        if abs(area(loop)) < MIN_AREA:
            continue
        pts = simplify_loop(loop, FACE_EPS)
        if len(pts) < 3:
            continue
        lx0, lx1 = min(p[0] for p in loop), max(p[0] for p in loop)
        ly0, ly1 = min(p[1] for p in loop), max(p[1] for p in loop)
        acc, n = [0, 0, 0], 0
        for y in range(int(ly0), int(ly1)):
            for x in range(int(lx0), int(lx1)):
                if face[y * w + x]:
                    c = rgb[y * w + x]
                    acc[0] += c[0]; acc[1] += c[1]; acc[2] += c[2]
                    n += 1
        col = tuple(min(255, int(a / max(n, 1) * 1.15)) for a in acc)
        paths.append(([gen(p) for p in pts], 1, col, 'face'))

    for line in chains(w, h, thin(w, h, edge)):
        if length(line) < MIN_EDGE:
            continue
        pts = dp([(p[0] + 0.5, p[1] + 0.5) for p in line], EDGE_EPS)
        paths.append(([gen(p) for p in pts], 0, (255, 240, 200), 'edge'))

    # the outlines left to right, so the beam goes once across
    paths.sort(key=lambda q: (q[3] != 'face', min(p[0] for p in q[0])))
    npts = sum(len(q[0]) for q in paths)
    nface = sum(1 for q in paths if q[3] == 'face')
    segs = sum(len(q[0]) - (0 if q[1] else 1) for q in paths)
    with open(OUT, 'w', newline='\n') as f:
        f.write('/* logo_shape.h - generated by tools/logo/logotrace.py from tools/logo/tempest_logo.png; do not edit.\n')
        f.write(' * The marquee\'s TEMPEST lettering in generator units, where the game\'s own logo lies when it has\n')
        f.write(' * come in: the letters\' outlines (closed, in their colours) first, then the middle lines of the\n')
        f.write(' * cream sides (open). */\n')
        f.write('#ifndef TP_LOGO_SHAPE_H\n#define TP_LOGO_SHAPE_H\n\n')
        f.write('#define TP_LOGO_BOTTOM %.1ff\n' % (TOP + (h - by0) * s))
        f.write('#define TP_LOGO_FACES %d\n#define TP_LOGO_PATHS %d\n#define TP_LOGO_SEGS %d\n\n' % (nface, len(paths), segs))
        f.write('typedef struct tp_logo_path { unsigned short first, n; unsigned char closed, r, g, b; } tp_logo_path;\n\n')
        f.write('static const float TP_LOGO_PT[%d][2] = {\n' % npts)
        for q in paths:
            f.write('   ' + ''.join(' { %.1ff, %.1ff },' % p for p in q[0]) + '\n')
        f.write('};\n\nstatic const tp_logo_path TP_LOGO_PATH[TP_LOGO_PATHS] = {\n')
        first = 0
        for q in paths:
            f.write('    { %d, %d, %d, %d, %d, %d },\n' % (first, len(q[0]), q[1], q[2][0], q[2][1], q[2][2]))
            first += len(q[0])
        f.write('};\n\n#endif\n')
    print('%s: %d outlines, %d middle lines, %d segments' % (OUT, nface, len(paths) - nface, segs))


if __name__ == '__main__':
    main()
