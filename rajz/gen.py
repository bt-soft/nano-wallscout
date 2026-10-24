import math, os

WIRE = '#0b4fa8'
PART = '#222'
TXT = '#111'


class Sheet:
    def __init__(s, w, h, title):
        s.w, s.h, s.title, s.o = w, h, title, []

    def wire(s, *p):
        s.o.append('<polyline points="%s" fill="none" stroke="%s" stroke-width="2" stroke-linejoin="round"/>' %
                   (' '.join('%g,%g' % q for q in p), WIRE))

    def line(s, *p, color=PART, w=2):
        s.o.append('<polyline points="%s" fill="none" stroke="%s" stroke-width="%g" stroke-linejoin="round"/>' %
                   (' '.join('%g,%g' % q for q in p), color, w))

    def dot(s, x, y):
        s.o.append('<circle cx="%g" cy="%g" r="4" fill="%s"/>' % (x, y, WIRE))

    def text(s, x, y, t, size=13, anchor='start', bold=False, color=TXT):
        s.o.append('<text x="%g" y="%g" font-size="%g" text-anchor="%s" fill="%s"%s>%s</text>' %
                   (x, y, size, anchor, color, ' font-weight="bold"' if bold else '', t))

    def lines(s, x, y, ts, size=13, anchor='start', color=TXT, dy=16):
        for i, t in enumerate(ts):
            s.text(x, y + i * dy, t, size, anchor, color=color, bold=(i == 0 and False))

    def gnd(s, x, y):
        s.line((x, y), (x, y + 8), color=WIRE)
        for i, hw in enumerate((12, 8, 4)):
            s.line((x - hw, y + 8 + i * 5), (x + hw, y + 8 + i * 5))

    def pwr(s, x, y, t):
        s.line((x - 10, y), (x + 10, y))
        s.text(x, y - 6, t, 13, 'middle', color='#b71c1c')

    def port(s, x, y, t, side='right'):
        # sheet connector label; text sits next to the wire end
        if side == 'right':
            s.text(x + 8, y + 4, t, 13, 'start', True, '#6a1b9a')
        else:
            s.text(x - 8, y + 4, t, 13, 'end', True, '#6a1b9a')

    def arrow(s, tx, ty, dx, dy, size=8):
        n = math.hypot(dx, dy); dx /= n; dy /= n
        bx, by = tx - dx * size, ty - dy * size
        px, py = -dy * size * 0.4, dx * size * 0.4
        s.o.append('<polygon points="%g,%g %g,%g %g,%g" fill="%s"/>' % (tx, ty, bx + px, by + py, bx - px, by - py, PART))

    def part(s, kind, p1, p2, label=(), side='auto', val_dx=0, below=False):
        (x1, y1), (x2, y2) = p1, p2
        L = math.hypot(x2 - x1, y2 - y1)
        mx, my = (x1 + x2) / 2, (y1 + y2) / 2
        ang = math.degrees(math.atan2(y2 - y1, x2 - x1))
        half = {'R': 14, 'C': 3, 'D': 10, 'TVS': 12, 'L': 18, 'SW': 16, 'BZ': 12}[kind]
        g = ['<g transform="translate(%g,%g) rotate(%g)" fill="none" stroke="%s" stroke-width="2">' % (mx, my, ang, PART)]
        g.append('<line x1="%g" y1="0" x2="%g" y2="0" stroke="%s"/>' % (-L / 2, -half, WIRE))
        g.append('<line x1="%g" y1="0" x2="%g" y2="0" stroke="%s"/>' % (half, L / 2, WIRE))
        if kind == 'R':
            g.append('<rect x="-14" y="-6" width="28" height="12" fill="#fff"/>')
        elif kind == 'C':
            g.append('<line x1="-3" y1="-11" x2="-3" y2="11"/><line x1="3" y1="-11" x2="3" y2="11"/>')
        elif kind == 'D':
            g.append('<polygon points="-8,-9 -8,9 8,0" fill="#fff"/><line x1="8" y1="-9" x2="8" y2="9"/>')
        elif kind == 'TVS':
            g.append('<polygon points="-8,-9 -8,9 8,0" fill="#fff"/><polyline points="4,-9 8,-9 8,9 12,9"/>')
        elif kind == 'L':
            g.append('<path d="M -18,0 a4.5,4.5 0 0 1 9,0 a4.5,4.5 0 0 1 9,0 a4.5,4.5 0 0 1 9,0 a4.5,4.5 0 0 1 9,0"/>')
        elif kind == 'SW':
            g.append('<circle cx="-14" cy="0" r="2.5" fill="#fff"/><circle cx="14" cy="0" r="2.5" fill="#fff"/>'
                     '<line x1="-16" y1="-9" x2="16" y2="-9"/><line x1="0" y1="-9" x2="0" y2="-15"/>')
        elif kind == 'BZ':
            g.append('<circle cx="0" cy="0" r="12" fill="#fff"/><line x1="-8" y1="0" x2="-3" y2="0"/>'
                     '<line x1="-5.5" y1="-2.5" x2="-5.5" y2="2.5"/>')
        g.append('</g>')
        s.o.append(''.join(g))
        horiz = abs(y2 - y1) < abs(x2 - x1)
        if label:
            if horiz and below:
                for i, t in enumerate(label):
                    s.text(mx + val_dx, my + 28 + i * 15, t, 12, 'middle', bold=(i == 0))
            elif horiz:
                for i, t in enumerate(reversed(label)):
                    s.text(mx + val_dx, my - 18 - i * 15, t, 12, 'middle', bold=(i == len(label) - 1))
            else:
                if side == 'left':
                    for i, t in enumerate(label):
                        s.text(mx - 18, my - 4 + i * 15, t, 12, 'end', bold=(i == 0))
                else:
                    for i, t in enumerate(label):
                        s.text(mx + 18, my - 4 + i * 15, t, 12, 'start', bold=(i == 0))

    def opamp(s, x, cy, name, pp, pm, po):
        s.o.append('<polygon points="%g,%g %g,%g %g,%g" fill="#fff" stroke="%s" stroke-width="2"/>' %
                   (x, cy - 38, x, cy + 38, x + 72, cy, PART))
        s.text(x + 6, cy - 11, '+', 16, 'start', True)
        s.text(x + 6, cy + 24, '−', 16, 'start', True)
        s.text(x + 30, cy + 5, name, 12, 'middle', True)
        s.text(x - 4, cy - 19, str(pp), 10, 'end', color='#666')
        s.text(x - 4, cy + 11, str(pm), 10, 'end', color='#666')
        s.text(x + 76, cy - 5, str(po), 10, 'start', color='#666')
        s.wire((x - 20, cy - 15), (x, cy - 15))
        s.wire((x - 20, cy + 15), (x, cy + 15))

    def save(s, path):
        head = ('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d" '
                'font-family="Segoe UI, Arial, sans-serif">' % (s.w, s.h, s.w, s.h))
        body = '<rect width="100%" height="100%" fill="#fff"/>'
        body += '<rect x="6" y="6" width="%d" height="%d" fill="none" stroke="#999"/>' % (s.w - 12, s.h - 12)
        body += '<text x="24" y="34" font-size="20" font-weight="bold" fill="#111">%s</text>' % s.title
        with open(path, 'w', encoding='utf-8') as f:
            f.write(head + body + ''.join(s.o) + '</svg>')


def mosfet(S, gx, cy):
    S.wire((gx, cy), (gx + 14, cy))
    S.line((gx + 14, cy - 18), (gx + 14, cy + 18))
    for a, b in ((-18, -10), (-4, 4), (10, 18)):
        S.line((gx + 20, cy + a), (gx + 20, cy + b))
    S.line((gx + 20, cy - 14), (gx + 40, cy - 14), color=WIRE)
    S.wire((gx + 40, cy - 14), (gx + 40, cy - 40))
    S.line((gx + 20, cy + 14), (gx + 40, cy + 14), color=WIRE)
    S.wire((gx + 40, cy), (gx + 40, cy + 40))
    S.line((gx + 20, cy), (gx + 40, cy), color=WIRE)
    S.arrow(gx + 20, cy, -1, 0)
    S.dot(gx + 40, cy + 14)
    return (gx + 40, cy - 40), (gx + 40, cy + 40)


def npn(S, bx, cy):
    S.wire((bx, cy), (bx - 14, cy))
    S.line((bx - 14, cy - 16), (bx - 14, cy + 16))
    S.line((bx - 14, cy - 8), (bx - 34, cy - 20), (bx - 34, cy - 34), color=WIRE)
    S.line((bx - 14, cy + 8), (bx - 34, cy + 20), (bx - 34, cy + 34), color=WIRE)
    S.arrow(bx - 30, cy + 17.6, -20, 12)
    return (bx - 34, cy - 34), (bx - 34, cy + 34)


# ---------------------------------------------------------------- 1. lap
def sheet1():
    S = Sheet(1400, 700, '1/3 – Táp és tekercs-meghajtó')
    S.pwr(0, -100, '')  # no-op placeholder removed below
    S.o.pop()
    R = 110
    # akku + kapcsolo
    S.wire((70, R), (110, R)); S.wire((70, R), (70, 230))
    for i in range(3):
        y = 230 + i * 16
        S.line((56, y), (84, y), w=3); S.line((62, y + 8), (78, y + 8), w=3)
    S.wire((70, 278), (70, 320)); S.gnd(70, 320)
    S.text(92, 232, '+', 14, bold=True)
    S.lines(92, 258, ['BT1', '3S Li-ion', '9 … 12,6 V'], 12)
    S.part('SW', (110, R), (170, R), ['S1', 'főkapcsoló'])
    S.wire((170, R), (1250, R))
    S.text(180, R - 30, 'VBAT', 14, bold=True, color='#b71c1c')
    # bulk kondik
    for x, lab in ((260, ['C1', '1000 µF / 25 V', 'low-ESR elko']), (420, ['C2', '10 µF', 'kerámia'])):
        S.part('C', (x, R), (x, 300), lab); S.dot(x, R); S.wire((x, 300), (x, 305)); S.gnd(x, 300)
    S.text(240, 360, 'C1, C2: közvetlenül a MOSFET / tekercs közelében', 12, color='#555')
    # 7805 ag
    Y = 560
    S.wire((600, R), (600, Y)); S.dot(600, R)
    S.wire((600, Y), (640, Y))
    S.part('C', (630, Y), (630, Y + 80), ['C3', '330 nF'], side='left'); S.dot(630, Y); S.gnd(630, Y + 80)
    S.o.append('<rect x="640" y="%d" width="80" height="60" fill="#fff" stroke="%s" stroke-width="2"/>' % (Y - 30, PART))
    S.text(680, Y + 5, '7805', 15, 'middle', True)
    S.text(646, Y - 34, 'IN', 11, color='#666'); S.text(712, Y - 34, 'OUT', 11, 'end', color='#666')
    S.wire((680, Y + 30), (680, Y + 70)); S.gnd(680, Y + 70)
    S.text(686, Y + 52, 'GND', 11, color='#666')
    S.wire((720, Y), (870, Y))
    S.part('C', (770, Y), (770, Y + 80), ['C4', '100 nF']); S.dot(770, Y); S.gnd(770, Y + 80)
    S.part('D', (830, Y), (930, Y), ['D1', '1N5819'])
    S.wire((930, Y), (1010, Y))
    S.text(1016, Y + 5, '5V  (Nano 5V láb)', 13, bold=True, color='#6a1b9a')
    S.text(830, Y + 44, '+5 V → kb. 4,7 V a diódán át', 12, color='#555')
    S.text(600, Y - 44, 'VBAT → 7805', 12, 'middle', color='#555')
    # tekercs + Rd
    xl, xr, xd = 1040, 1140, 1260
    D = 330
    S.part('L', (xl, R), (xl, D), ['L1 (Tx D-tekercs)', '1 mH, 6 × 5 cm'], side='left')
    S.part('R', (xr, R), (xr, D), ['Rd', '1 kΩ + 1 kΩ', 'trimmer sorosan'])
    S.dot(xl, R); S.dot(xr, R)
    S.wire((xl, D), (xd, D)); S.dot(xl, D); S.dot(xr, D)
    S.part('D', (xd, D), (xd, 410), ['D2', 'UF4007'])
    for i, name in enumerate(('DZ1', 'DZ2', 'DZ3')):
        S.part('TVS', (xd, 470 + 60 * i), (xd, 410 + 60 * i), [name, '12–13 V', '≥ 1 W'])
    S.gnd(xd, 590)
    S.dot(xd, D)
    # mosfet
    gx, cy = 1000, 440
    dpin, spin = mosfet(S, gx, cy)
    S.wire((xl, D), (dpin[0], dpin[1]))
    S.wire(spin, (spin[0], spin[1] + 14)); S.gnd(spin[0], spin[1] + 14)
    S.text(gx + 56, cy + 4, 'Q1 IRLZ44N', 13, bold=True)
    # gate halozat
    S.text(860, cy + 5, 'D2 ►', 13, 'end', True, '#6a1b9a')
    S.wire((866, cy), (880, cy)); S.part('R', (866, cy), (950, cy), ['R1 47 Ω'])
    S.wire((950, cy), (gx, cy)); S.dot(960, cy)
    S.part('R', (960, cy), (960, cy + 80), ['R2 10 kΩ'], side='left'); S.gnd(960, cy + 80)
    S.text(24, 420, 'Fontos:', 13, bold=True)
    S.lines(24, 438, ['Q1, D2 és DZ1–DZ3 a lehető legrövidebb,', 'vastag huzalozással egymás mellé;',
                      'a zener-sor földje közvetlenül Q1 source-hoz.',
                      'Rx (L2) a 2. lapon: az L1-gyel DD-ben átfedve.'], 12)
    S.text(24, 660, 'D2 + DZ1–DZ3: a flyback-et kb. 36–39 V-ra lecsípi (Q1 Vds,max = 55 V); a zener-sor feszültsége nagy áramnál megnő, ezért az áramot (T_ON) szkóppal ellenőrizd.', 12, color='#555')
    S.text(24, 678, 'Piros = tápsín, kék = vezeték, lila = másik lapra / Nanóra menő jel.', 12, color='#555')
    S.save(OUT + '1_tap_es_meghajto.svg')


# ---------------------------------------------------------------- 2. lap
def sheet2():
    S = Sheet(1400, 860, '2/3 – Analóg jelút (MCP6004)')
    # 5VA
    S.text(24, 84, '+5 V (Nano 5V láb) ►', 13, bold=True, color='#6a1b9a')
    S.part('R', (230, 100), (310, 100), ['R6 10 Ω'])
    S.wire((24 + 190, 100), (230, 100)); S.wire((310, 100), (900, 100))
    S.text(206, 104, '', 1)
    for x, lab in ((360, ['C6', '10 µF']), (440, ['C7', '100 nF'])):
        S.part('C', (x, 100), (x, 190), lab); S.dot(x, 100); S.gnd(x, 190)
    S.text(560, 90, '5VA', 14, bold=True, color='#b71c1c')
    S.text(560, 120, 'Az ADC referenciája a Nano Vcc-je (AREF üresen marad).', 12, color='#555')
    S.text(560, 138, '◄► MCP6004: V+ = 4. láb, V− = 11. láb (GND), 100 nF a lábaknál', 12, color='#555')
    S.dot(500, 100)

    BUS = 600
    # bemeneti halozat: Rx tekercs a Vmid-re kotve, csillapitas, soros vedoellenallas
    sy = 400
    xL, xX = 200, 330
    S.text(24, 250, 'Rx D-tekercs (L2): egyik vége a Vmid-re kötve, a másik a bemenet.', 12, color='#555')
    S.text(24, 266, 'Az L1 (Tx) és L2 DD-ben átfedve: a közvetlen áthallás ≈ 0.', 12, color='#555')
    S.part('L', (xL, sy), (xL, BUS), ['L2 (Rx D-tekercs)', '1 mH, 6 × 5 cm'], side='left')
    S.dot(xL, BUS)
    S.wire((xL, BUS), (xX, BUS))
    S.text(xL + 6, BUS + 18, 'Vmid', 12, bold=True, color='#b71c1c')
    S.wire((xL, sy), (xX, sy)); S.dot(xL, sy); S.dot(xX, sy)
    S.part('R', (xX, sy), (xX, BUS), ['R3', '2,2 kΩ trimmer', '(Rx csillapítás)'])
    S.dot(xX, BUS)
    S.part('R', (xX, sy), (xX + 110, sy), ['R5 10 kΩ', '(soros védelem)'])
    S.dot(xX, sy)
    S.wire((xX, sy), (xX, 330))
    S.part('R', (xX, 330), (xX + 90, 330), ['R15 10 kΩ'])
    S.text(xX + 96, 335, '► A3 (nyers Rx, DD durva beállítás)', 12, bold=True, color='#6a1b9a')

    # U1A
    x1, cy = 560, 415
    S.wire((xX + 110, sy), (x1 - 20, sy))
    S.opamp(x1, cy, 'U1A', 3, 2, 1)
    xo = x1 + 72
    S.wire((xo, cy), (xo + 30, cy)); S.dot(xo + 20, cy)
    S.part('R', (xo + 20, cy), (xo + 20, 300), ['R14 330 Ω'], side='right')
    S.text(xo + 28, 285, '► A2 (U1A ×10, DD beállító mód)', 12, bold=True, color='#6a1b9a')
    S.wire((xo + 20, cy), (xo + 20, cy + 55))
    S.part('R', (xo + 20, cy + 55), (x1 - 30, cy + 55), ['Rf1 91 kΩ'], below=True)
    S.wire((x1 - 30, cy + 55), (x1 - 30, cy + 15)); S.wire((x1 - 30, cy + 15), (x1 - 20, cy + 15))
    S.dot(x1 - 30, cy + 55)
    S.part('R', (x1 - 30, cy + 55), (x1 - 30, BUS), ['Rg1 10 kΩ'], side='left'); S.dot(x1 - 30, BUS)
    S.text(x1 + 25, cy + 110, '×10,1', 13, 'middle', True)

    # U1B
    x2 = 810
    S.wire((xo + 20, cy), (xo + 50, cy), (xo + 50, sy))
    S.part('R', (xo + 50, sy), (x2 - 20, sy), ['R7 1 kΩ'], val_dx=10)
    S.opamp(x2, cy, 'U1B', 5, 6, 7)
    xo2 = x2 + 72
    S.wire((xo2, cy), (xo2 + 30, cy)); S.dot(xo2 + 20, cy)
    S.wire((xo2 + 20, cy), (xo2 + 20, cy + 55))
    S.part('R', (xo2 + 20, cy + 55), (x2 - 30, cy + 55), ['Rf2 91 kΩ'], below=True)
    S.wire((x2 - 30, cy + 55), (x2 - 30, cy + 15)); S.wire((x2 - 30, cy + 15), (x2 - 20, cy + 15))
    S.dot(x2 - 30, cy + 55)
    S.part('R', (x2 - 30, cy + 55), (x2 - 30, BUS), ['Rg2 10 kΩ'], side='left'); S.dot(x2 - 30, BUS)
    S.text(x2 + 25, cy + 110, '×10,1', 13, 'middle', True)

    # U1D
    x3 = 1060
    S.wire((xo2 + 20, cy), (xo2 + 50, cy), (xo2 + 50, sy), (x3 - 20, sy))
    S.opamp(x3, cy, 'U1D', 12, 13, 14)
    xo3 = x3 + 72
    S.wire((xo3, cy), (xo3 + 30, cy)); S.dot(xo3 + 20, cy)
    S.wire((xo3 + 20, cy), (xo3 + 20, cy + 55), (x3 - 30, cy + 55), (x3 - 30, cy + 15), (x3 - 20, cy + 15))
    S.text(x3 + 25, cy + 90, 'követő ×1', 13, 'middle', True)
    S.part('R', (xo3 + 30, cy), (xo3 + 120, cy), ['R8 330 Ω'])
    S.wire((xo3 + 120, cy), (xo3 + 150, cy)); S.dot(xo3 + 140, cy)
    S.part('C', (xo3 + 140, cy), (xo3 + 140, cy + 70), ['C8 1 nF']); S.gnd(xo3 + 140, cy + 70)
    S.text(xo3 + 156, cy + 5, '► A0 (ADC0)', 13, bold=True, color='#6a1b9a')
    S.text(xo3 + 156, cy - 10, 'Nano A0 láb', 11, color='#555')

    # Vmid sin
    S.wire((xX, BUS), (1240, BUS))
    S.text(1245, BUS + 4, 'Vmid', 14, bold=True, color='#b71c1c')

    # Vmid generator
    gx = 170
    S.pwr(gx, 640, '5VA')
    S.part('R', (gx, 640), (gx, 730), ['R9 10 kΩ'], side='left')
    S.dot(gx, 730)
    S.part('R', (gx, 730), (gx, 810), ['R10 10 kΩ'], side='left'); S.gnd(gx, 810)
    S.wire((gx, 730), (gx + 60, 730)); S.dot(gx + 60, 730)
    S.part('C', (gx + 60, 730), (gx + 60, 810), ['C9 1 µF']); S.gnd(gx + 60, 810)
    ux, ucy = 330, 745
    S.wire((gx + 60, 730), (ux - 20, 730))
    S.opamp(ux, ucy, 'U1C', 10, 9, 8)
    uo = ux + 72
    S.wire((uo, ucy), (uo + 30, ucy)); S.dot(uo + 20, ucy)
    S.wire((uo + 20, ucy), (uo + 20, ucy + 55), (ux - 30, ucy + 55), (ux - 30, ucy + 15), (ux - 20, ucy + 15))
    S.wire((uo + 20, ucy), (uo + 20, BUS)); S.wire((uo + 20, BUS), (xX, BUS)) if False else None
    S.wire((uo + 20, BUS), (xX + 0, BUS)) if uo + 20 < xX else S.wire((uo + 20, ucy), (uo + 20, BUS))
    S.text(uo + 34, ucy + 5, 'Vmid', 13, bold=True, color='#b71c1c')
    S.text(ux - 10, 840, 'Vmid = 5VA / 2 ≈ 2,5 V (puffer)', 12, color='#555')
    S.text(700, 700, 'Összerősítés: 10,1 × 10,1 ≈ 100×.', 13, bold=True)
    S.lines(700, 720, ['A jel a Vmid (2,5 V) körül mindkét irányba billenhet; az S1 − S2 különbség kiejti az offsetet.',
                       'Ha a kijelzőn TULVEZERLES van: Rf2 → 47 kΩ (×5,7), vagy javítsd a DD átfedést.',
                       'R5 (10 kΩ) az MCP6004 belső ESD-diódáival együtt védi a bemenetet, külön dióda nem kell.'], 12, color='#555')
    S.save(OUT + '2_analog_jelut.svg')


# ---------------------------------------------------------------- 3. lap
def sheet3():
    S = Sheet(1300, 760, '3/3 – Arduino Nano bekötése')
    bx0, bx1, by0, by1 = 500, 780, 70, 650
    S.o.append('<rect x="%d" y="%d" width="%d" height="%d" fill="#e8f5e9" stroke="%s" stroke-width="2"/>' %
               (bx0, by0, bx1 - bx0, by1 - by0, PART))
    S.text((bx0 + bx1) / 2, 360, 'Arduino', 15, 'middle', True)
    S.text((bx0 + bx1) / 2, 380, 'Nano', 15, 'middle', True)
    S.text((bx0 + bx1) / 2, 400, '(ATmega328P)', 12, 'middle')

    def lp(y, t):
        S.text(bx0 + 6, y + 4, t, 12, 'start', True)

    def rp(y, t):
        S.text(bx1 - 6, y + 4, t, 12, 'end', True)

    # bal
    lp(130, 'D2'); S.wire((bx0, 130), (430, 130))
    S.text(424, 135, 'GATE ► 1. lap (R1)', 13, 'end', True, '#6a1b9a')
    lp(190, 'A4 / SDA'); S.wire((bx0, 190), (360, 190))
    lp(230, 'A5 / SCL'); S.wire((bx0, 230), (360, 230))
    S.o.append('<rect x="240" y="150" width="120" height="140" fill="#fff" stroke="%s" stroke-width="2"/>' % PART)
    S.text(300, 215, 'SSD1306', 14, 'middle', True)
    S.text(300, 233, '128×64 OLED', 12, 'middle')
    S.text(300, 251, 'I²C (0x3C)', 12, 'middle')
    S.text(366, 165, 'VCC', 11, color='#666')
    S.text(366, 183, 'SDA', 11, color='#666'); S.text(366, 223, 'SCL', 11, color='#666'); S.text(366, 260, 'GND', 11, color='#666')
    S.wire((360, 170), (400, 170)); S.text(406, 175, '5V', 12, color='#6a1b9a')
    S.wire((360, 270), (400, 270)); S.gnd(400, 270)
    S.text(240, 316, 'a 4,7 kΩ I²C felhúzók a modulon vannak', 11, color='#555')
    lp(350, 'D4'); S.wire((bx0, 350), (440, 350))
    S.part('SW', (440, 350), (340, 350), ['S2  NULLÁZÁS', '(hosszan: DD beállító)']); S.wire((340, 350), (330, 350)); S.gnd(330, 350)
    lp(410, 'D5'); S.wire((bx0, 410), (440, 410))
    S.part('SW', (440, 410), (340, 410), ['S3  KALIBRÁLÁS']); S.wire((340, 410), (330, 410)); S.gnd(330, 410)
    S.text(230, 440, 'belső felhúzó, gomb a GND-re', 11, color='#555')
    lp(580, 'D11'); S.wire((bx0, 580), (500, 580))
    S.part('R', (500, 580), (440, 580), ['R11 1 kΩ'])
    qb, qc = 400, 580
    c, e = npn(S, qb, qc)
    S.wire((440, 580), (qb, qc))
    S.text(qb - 80, qc + 64, 'Q2 BC547', 12, bold=True)
    S.gnd(e[0], e[1])
    # csipogo
    top = 480
    S.pwr(c[0] + 25, top - 10, '5V')
    S.wire((c[0] + 25, top - 10), (c[0] + 25, top))
    S.part('BZ', (c[0] + 25, top), (c[0] + 25, c[1]), ['BZ1', 'passzív', 'csipogó'])
    S.wire((c[0] + 25, c[1]), (c[0], c[1]))
    S.part('D', (c[0] - 25, c[1]), (c[0] - 25, top), ['D5', '1N4148'], side='left')
    S.wire((c[0] - 25, top), (c[0] + 25, top)); S.dot(c[0] + 25, top); S.dot(c[0] + 25, c[1]); S.dot(c[0] - 25, c[1])
    S.wire((c[0] - 25, c[1]), (c[0], c[1]))

    # jobb
    rp(110, 'A0 / ADC0'); S.wire((bx1, 110), (840, 110))
    S.text(846, 115, '◄ U1D kimenet, R8 (2. lap)', 13, bold=True, color='#6a1b9a')
    rp(130, 'A3 / ADC3'); S.wire((bx1, 130), (840, 130))
    S.text(846, 135, '◄ nyers Rx, R15 (2. lap), DD durva', 13, bold=True, color='#6a1b9a')
    rp(150, 'A2 / ADC2'); S.wire((bx1, 150), (840, 150))
    S.text(846, 155, '◄ U1A kimenet, R14 (2. lap), DD beállítás', 13, bold=True, color='#6a1b9a')
    rp(190, 'A1 / ADC1'); S.wire((bx1, 190), (840, 190)); S.dot(840, 190)
    S.part('R', (840, 190), (980, 190), ['R12 100 kΩ'])
    S.text(986, 195, 'VBAT (1. lap)', 13, bold=True, color='#6a1b9a')
    S.part('R', (840, 190), (840, 280), ['R13 33 kΩ']); S.gnd(840, 280)
    S.part('C', (900, 190), (900, 280), ['C10', '100 nF']); S.dot(900, 190); S.gnd(900, 280)
    S.text(940, 300, 'akkufeszültség: Vbat × 33/133', 11, color='#555')
    rp(330, 'AREF'); S.text(846, 335, 'üresen marad (belső referencia = Vcc)', 12, color='#555')
    rp(380, '5V'); S.wire((bx1, 380), (840, 380))
    S.text(846, 385, '◄ D1 katód (1. lap) ► R6 szűrő (2. lap), OLED VCC', 13, bold=True, color='#6a1b9a')
    rp(470, 'VIN'); S.text(846, 475, 'üresen marad', 12, color='#555')
    rp(560, 'GND'); S.wire((bx1, 560), (820, 560)); S.gnd(820, 560)
    S.text(840, 620, 'Az USB-t csak programozáskor dugd be;', 12, color='#555')
    S.text(840, 636, 'a D1 diódával a 7805 nem kap vissza áramot.', 12, color='#555')
    S.text(24, 730, 'Minden GND közös (egy csillagpont a Q1 source-nál).', 12, color='#555')
    S.save(OUT + '3_nano_bekotes.svg')


OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '')
if __name__ == '__main__':
    sheet1(); sheet2(); sheet3()



