import argparse
from PIL import Image, ImageDraw, ImageFont
import os
import sys

# ---------------------------------------------------------------------------
# Unified canvas & layout constants
# All output PNGs (template + sprites) are CANVAS_W x CANVAS_H so they
# overlay perfectly when the plugin draws them at the same screen position.
# ---------------------------------------------------------------------------

CANVAS_W = 800
CANVAS_H = 420

UNIT = 72    # 1-unit key width/height in pixels
PAD  = 5     # pixel gap between key boxes
SX   = 40    # keyboard left margin
SY   = 40    # keyboard top margin

# Mouse geometry (placed to the right of the keyboard)
MOUSE_X     = 590   # left edge of mouse body
MOUSE_Y     = 95    # top  edge of mouse body
MOUSE_W     = 130
MOUSE_H     = 215
MOUSE_BTN_H = 108   # height of the LMB/RMB button zone
MOUSE_MID_X = MOUSE_X + MOUSE_W // 2   # centre split line

# ---------------------------------------------------------------------------
# Key & mouse sprite rects  { key_name: (x, y, w, h) }
# These positions MUST match the keyboard_template.png layout exactly.
# ---------------------------------------------------------------------------

def get_key_rects(layout='full'):
    r = {}

    # Row 0 — Esc, 1-5
    r0y = SY
    r['esc']   = (SX,               r0y, UNIT-PAD, UNIT-PAD)
    for i, k in enumerate(['1','2','3','4','5']):
        r[k] = (SX + UNIT*(i+1), r0y, UNIT-PAD, UNIT-PAD)

    # Row 1 — Tab, Q-T
    r1y  = SY + UNIT
    tabw = int(UNIT*1.5) - PAD
    r['tab'] = (SX, r1y, tabw, UNIT-PAD)
    for i, k in enumerate(['q','w','e','r','t']):
        r[k] = (SX + int(UNIT*1.5) + i*UNIT, r1y, UNIT-PAD, UNIT-PAD)

    # Row 2 — Caps, A-G
    r2y  = SY + UNIT*2
    capw = int(UNIT*1.75) - PAD
    r['caps'] = (SX, r2y, capw, UNIT-PAD)
    for i, k in enumerate(['a','s','d','f','g']):
        r[k] = (SX + int(UNIT*1.75) + i*UNIT, r2y, UNIT-PAD, UNIT-PAD)

    # Row 3 — Shift, Z-B
    r3y  = SY + UNIT*3
    shfw = int(UNIT*2.25) - PAD
    r['shift'] = (SX, r3y, shfw, UNIT-PAD)
    for i, k in enumerate(['z','x','c','v','b']):
        r[k] = (SX + int(UNIT*2.25) + i*UNIT, r3y, UNIT-PAD, UNIT-PAD)

    # Row 4 — Ctrl, Alt, Space
    r4y  = SY + UNIT*4 + 6
    kh4  = UNIT - PAD - 10          # shorter height for bottom row
    ctlw = int(UNIT*1.25) - PAD
    altw = int(UNIT*1.25) - PAD
    spaw = int(UNIT*3.5)  - PAD
    r['ctrl']  = (SX,                       r4y, ctlw, kh4)
    r['alt']   = (SX + int(UNIT*1.25),      r4y, altw, kh4)
    r['space'] = (SX + int(UNIT*2.5),       r4y, spaw, kh4)

    # Mouse buttons — inset inside the mouse body outline
    INS = 4
    LBW = MOUSE_MID_X - MOUSE_X - INS*2        # left-button width inside mouse
    LBH = MOUSE_BTN_H - INS*2
    r['mouse_left']  = (MOUSE_X   + INS, MOUSE_Y + INS,          LBW, LBH)
    r['mouse_right'] = (MOUSE_MID_X + INS, MOUSE_Y + INS, LBW - 1, LBH)
    
    # Side buttons (MB5 forward, MB4 back) pushed to the left so they look mostly detached
    r['mouse_5'] = (MOUSE_X - 18, MOUSE_Y + 60, 12, 32)
    r['mouse_4'] = (MOUSE_X - 18, MOUSE_Y + 100, 12, 32)

    if layout == 'wasd':
        wasd_keys = {'w','a','s','d','q','e','r','f','shift','ctrl','space','esc','tab','z','x','c','1','2','3','4','5','caps','alt','g','v','b','t'}
        return {k: v for k, v in r.items() if k in wasd_keys}
    elif layout == 'mouse':
        mouse_keys = {'mouse_left', 'mouse_right', 'mouse_4', 'mouse_5'}
        return {k: v for k, v in r.items() if k in mouse_keys}
        
    return r


def get_bounding_box(key_rects, layout='full'):
    # ---- Compute panel bounding box (keys + mouse) ----
    MARGIN = 14
    kb_keys = [v for k, v in key_rects.items()
               if not k.startswith('mouse_')]
    
    if kb_keys:
        all_x0 = min(x        for x,y,w,h in kb_keys)
        all_y0 = min(y        for x,y,w,h in kb_keys)
        all_x1 = max(x+w      for x,y,w,h in kb_keys)
        all_y1 = max(y+h      for x,y,w,h in kb_keys)
    else:
        all_x0, all_y0, all_x1, all_y1 = float('inf'), float('inf'), 0, 0

    if layout in ('full', 'mouse'):
        # Include mouse body
        MOUSE_X = 590
        MOUSE_Y = 95
        MOUSE_W = 130
        MOUSE_H = 215
        all_x1 = max(all_x1, MOUSE_X + MOUSE_W)
        all_y0 = min(all_y0, MOUSE_Y)
        all_y1 = max(all_y1, MOUSE_Y + MOUSE_H)
        all_x0 = min(all_x0, MOUSE_X)

    bx0, by0 = all_x0 - MARGIN, all_y0 - MARGIN
    bx1, by1 = all_x1 + MARGIN, all_y1 + MARGIN
    return bx0, by0, bx1, by1


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def load_fonts():
    try:
        return (ImageFont.truetype("arialbd.ttf", 18),
                ImageFont.truetype("arial.ttf",   13))
    except OSError:
        f = ImageFont.load_default()
        return f, f


def draw_centred_text(draw, x, y, w, h, text, font, color):
    bbox = draw.textbbox((0, 0), text, font=font)
    tw, th = bbox[2]-bbox[0], bbox[3]-bbox[1]
    draw.text((x+(w-tw)//2, y+(h-th)//2), text, fill=color, font=font)


# ---------------------------------------------------------------------------
# Template generator
# ---------------------------------------------------------------------------

def generate_keyboard_template(output_dir, layout='full'):
    KEY_RECTS = get_key_rects(layout)
    font_lg, font_sm = load_fonts()

    # Base design colors now use 255 alpha so the in-game slider has full control
    KEY_BG   = (35,  35,  35,  255)
    KEY_OUT  = (155, 155, 155, 225)
    LABEL    = (225, 225, 225, 255)
    MOUSE_BG = (25,  25,  25,  255)
    PANEL_BG = (18,  18,  18,  255)
    PANEL_OT = (75,  75,  75,  200)
    RADIUS   = 6

    # ---- Compute panel bounding box (keys + mouse) ----
    bx0, by0, bx1, by1 = get_bounding_box(KEY_RECTS, layout)

    # We will generate two separate layers: BG (fades) and Outlines (static)
    img_bg      = Image.new('RGBA', (CANVAS_W, CANVAS_H), (0, 0, 0, 0))
    img_outline = Image.new('RGBA', (CANVAS_W, CANVAS_H), (0, 0, 0, 0))
    
    d_bg  = ImageDraw.Draw(img_bg)
    d_out = ImageDraw.Draw(img_outline)

    # ---- Background panel ----
    d_bg.rounded_rectangle([bx0, by0, bx1, by1], radius=12, fill=PANEL_BG)
    d_out.rounded_rectangle([bx0, by0, bx1, by1], radius=12, outline=PANEL_OT)

    # ---- Keyboard keys ----
    label_map = {
        'esc':'Esc','tab':'Tab','caps':'Caps','shift':'Shift',
        'ctrl':'Ctrl','alt':'Alt','space':'Space',
        '1':'1','2':'2','3':'3','4':'4','5':'5',
        'q':'Q','w':'W','e':'E','r':'R','t':'T',
        'a':'A','s':'S','d':'D','f':'F','g':'G',
        'z':'Z','x':'X','c':'C','v':'V','b':'B',
    }
    modifier_keys = {'esc','tab','caps','shift','ctrl','alt','space'}

    for key, (x, y, w, h) in KEY_RECTS.items():
        if str(key).startswith('mouse_'):
            continue
        font = font_sm if key in modifier_keys else font_lg
        d_bg.rounded_rectangle([x, y, x+w, y+h], radius=RADIUS, fill=KEY_BG)
        d_out.rounded_rectangle([x, y, x+w, y+h], radius=RADIUS, outline=KEY_OUT)
        draw_centred_text(d_out, x, y, w, h, label_map.get(str(key), str(key).upper()), font, LABEL)

    if layout in ('full', 'mouse'):
        # Mouse body
        d_bg.rounded_rectangle([MOUSE_X, MOUSE_Y, MOUSE_X+MOUSE_W, MOUSE_Y+MOUSE_H], radius=42, fill=MOUSE_BG)
        d_out.rounded_rectangle([MOUSE_X, MOUSE_Y, MOUSE_X+MOUSE_W, MOUSE_Y+MOUSE_H], radius=42, outline=KEY_OUT)

        # Mouse side buttons
        if 'mouse_5' in KEY_RECTS:
            m5 = KEY_RECTS['mouse_5']
            d_bg.rounded_rectangle([m5[0], m5[1], m5[0]+m5[2], m5[1]+m5[3]], radius=3, fill=MOUSE_BG)
            d_out.rounded_rectangle([m5[0], m5[1], m5[0]+m5[2], m5[1]+m5[3]], radius=3, outline=KEY_OUT)
            
            m4 = KEY_RECTS['mouse_4']
            d_bg.rounded_rectangle([m4[0], m4[1], m4[0]+m4[2], m4[1]+m4[3]], radius=3, fill=MOUSE_BG)
            d_out.rounded_rectangle([m4[0], m4[1], m4[0]+m4[2], m4[1]+m4[3]], radius=3, outline=KEY_OUT)

        # Divider: vertical centre split (button zone only)
        d_out.line([(MOUSE_MID_X, MOUSE_Y+3), (MOUSE_MID_X, MOUSE_Y+MOUSE_BTN_H-3)], fill=(110, 110, 110, 200), width=2)
        # Divider: horizontal — separates buttons from palm rest
        d_out.line([(MOUSE_X+10, MOUSE_Y+MOUSE_BTN_H), (MOUSE_X+MOUSE_W-10, MOUSE_Y+MOUSE_BTN_H)], fill=(110, 110, 110, 200), width=2)

    # Crop both images
    img_bg = img_bg.crop((bx0, by0, bx1, by1))
    img_outline = img_outline.crop((bx0, by0, bx1, by1))

    # Save components
    img_bg.save(os.path.join(output_dir, 'keyboard_bg.png'))
    img_outline.save(os.path.join(output_dir, 'keyboard_outlines.png'))
    
    # Save combined template for user reference (with alpha mimicking old default)
    img_combined = Image.new('RGBA', img_bg.size)
    bg_with_alpha = img_bg.copy()
    bg_with_alpha.putalpha(bg_with_alpha.getchannel('A').point(lambda i: int(i * 0.65)))
    img_combined.paste(bg_with_alpha, (0,0))
    img_combined.paste(img_outline, (0,0), mask=img_outline)
    img_combined.save(os.path.join(output_dir, 'keyboard_template.png'))
    
    print(f"Base layers saved  ({CANVAS_W}×{CANVAS_H} px boxed)")
    print(f"  Open in Photoshop/GIMP, draw your design, export as PNG, drop back here.")


# ---------------------------------------------------------------------------
# Per-key sprite generator  (pressed = green highlight only)
# ---------------------------------------------------------------------------

def generate_key_sprites(output_dir, layout='full'):
    KEY_RECTS = get_key_rects(layout)
    font_lg, font_sm = load_fonts()

    # For Phase 2, pressed sprites MUST be pure solid white. 
    # Unreal Engine's canvas.SetColor() multiplies this texture by the chosen color. 
    # If this was red (1,0,0) and the user chose blue (0,0,1), it would render black (0,0,0).
    # White (1,1,1) allows it to tint to any color perfectly.
    PRESS_FILL   = (255, 255, 255, 255)
    PRESS_OUT    = (255, 255, 255, 255)

    label_map = {
        'space': 'Space', 'shift': 'Shift',
        'esc': 'Esc', 'tab': 'Tab', 'caps': 'Caps', 
        'ctrl': 'Ctrl', 'alt': 'Alt'
    }

    for key, (x, y, w, h) in KEY_RECTS.items():
        img_p = Image.new('RGBA', (CANVAS_W, CANVAS_H), (0, 0, 0, 0))
        d_p   = ImageDraw.Draw(img_p)
        
        if key in ('mouse_left', 'mouse_right'):
            # Create a full canvas with the entire mouse body drawn in green
            temp = Image.new('RGBA', (CANVAS_W, CANVAS_H), (0, 0, 0, 0))
            temp_draw = ImageDraw.Draw(temp)
            temp_draw.rounded_rectangle([MOUSE_X, MOUSE_Y, MOUSE_X+MOUSE_W, MOUSE_Y+MOUSE_H], 
                                        radius=42, fill=PRESS_FILL, outline=PRESS_OUT, width=2)
            
            # Create a mask that ONLY reveals the specific button's bounding box
            mask = Image.new('L', (CANVAS_W, CANVAS_H), 0)
            mask_draw = ImageDraw.Draw(mask)
            mask_draw.rectangle([x, y, x+w, y+h], fill=255)
            
            # Apply the mask
            img_p.paste(temp, (0, 0), mask=mask)
            
            # Since the crop removes the inner border lines (where LMB meets RMB and palm reset), we draw them back
            if key == 'mouse_left':
                d_p.line([(x+w-1, y), (x+w-1, y+h)], fill=PRESS_OUT, width=2)     # Right edge
                d_p.line([(x, y+h-1), (x+w, y+h-1)], fill=PRESS_OUT, width=2)     # Bottom edge
            elif key == 'mouse_right':
                d_p.line([(x, y), (x, y+h)], fill=PRESS_OUT, width=2)             # Left edge
                d_p.line([(x, y+h-1), (x+w, y+h-1)], fill=PRESS_OUT, width=2)     # Bottom edge
                
        elif key in ('mouse_4', 'mouse_5'):
            d_p.rounded_rectangle([x, y, x+w, y+h], radius=3, fill=PRESS_FILL, outline=PRESS_OUT)
        else:
            lbl = label_map.get(str(key), str(key).upper())
            font = font_sm if len(lbl) > 1 else font_lg
            d_p.rounded_rectangle([x, y, x+w, y+h], radius=6, fill=PRESS_FILL, outline=PRESS_OUT)
            draw_centred_text(d_p, x, y, w, h, lbl, font, (0, 0, 0, 255))
            
        # Crop the sprite to exactly match the base template layer bounding box
        bx0, by0, bx1, by1 = get_bounding_box(KEY_RECTS, layout)
        img_p = img_p.crop((bx0, by0, bx1, by1))

        img_p.save(os.path.join(output_dir, f'{key}_pressed.png'))

    print("Key pressed sprites generated.")
    
    # ---------------------------------------------------------------------------
    # Write instructions file for users
    # ---------------------------------------------------------------------------
    bx0, by0, bx1, by1 = get_bounding_box(KEY_RECTS, layout)
    crop_w = bx1 - bx0
    crop_h = by1 - by0

    marker_name = f"DIMENSIONS_{crop_w}x{crop_h}"
    marker_path = os.path.join(output_dir, marker_name)
    with open(marker_path, 'w') as f:
        pass


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Generate Custom KBM Overlay templates.")
    parser.add_argument('output_dir', help="Output directory")
    parser.add_argument('--layout', choices=['full', 'wasd', 'mouse'], default='full', help="Layout profile to generate")
    args = parser.parse_args()

    out = args.output_dir
    if args.layout == 'wasd':
        out = os.path.join(out, 'layouts', 'wasd')
    elif args.layout == 'mouse':
        out = os.path.join(out, 'layouts', 'mouse')
        
    os.makedirs(out, exist_ok=True)

    generate_key_sprites(out, args.layout)
    generate_keyboard_template(out, args.layout)
        
    print('All done!')
