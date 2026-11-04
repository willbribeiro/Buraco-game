from PIL import Image

font = open('port/assets/font8x16.bin', 'rb').read()
lines = [
    'BURACO - 8x16 VGA font dump',
    'ABCDEFGHIJKLMNOPQRSTUVWXYZ',
    'abcdefghijklmnopqrstuvwxyz',
    '0123456789 !"#$%&()*+,-./:;<=>?',
    '\xc9\xcd\xbb\xba\xc8\xbc A234567891JQKA',
]
im = Image.new('RGB', (8 * 32, 16 * len(lines)), (0, 0, 0))
px = im.load()
for r, s in enumerate(lines):
    for c, chx in enumerate(s[:32]):
        o = ord(chx) * 16
        for gy in range(16):
            b = font[o + gy]
            for gx in range(8):
                if b & (0x80 >> gx):
                    px[c * 8 + gx, r * 16 + gy] = (255, 255, 255)
im.resize((8 * 32 * 2, 16 * len(lines) * 2), Image.NEAREST).save('shots/21_font.png')
print('ok')
