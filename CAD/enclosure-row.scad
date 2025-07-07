include <BOSL2/std.scad>

$fn=256;
epsilon=0.01;

LAYER = 0.2; // Layer height for 3D printing
WALL_THICKNESS = 2.5;
CHAMFER = 0.5;

TOP_ANGLE = 25;
TOP_WIDTH = 150;
TOP_DEPTH = 100;
TOP_HEIGHT_FRONT = 20;
TOP_HEIGHT_BACK = TOP_HEIGHT_FRONT + tan(TOP_ANGLE) * TOP_DEPTH;
PANEL_DEPTH = TOP_DEPTH / cos(TOP_ANGLE);

BASE_OFFSET = 0.2;
BASE = [TOP_WIDTH-2*WALL_THICKNESS-2*BASE_OFFSET, TOP_DEPTH-2*WALL_THICKNESS-2*BASE_OFFSET, WALL_THICKNESS];

REINF = 4;

SWITCH_DIAM = 19;
SWITCH_OFFSET = 5;
SWITCH_SPREAD = 4.5; // 1/nth of the top width

SCREEN_W = 52;
SCREEN_H = 40;
SCREEN_BOARD = [71, 44, REINF+WALL_THICKNESS-1+2*epsilon];
SCREEN_DEPTH = 14;
SCREEN_BOARD_OFFSET = 2.75;
SCREEN_OFFSET = TOP_WIDTH/SWITCH_SPREAD; // Offset for screen from the centreline

DIAL_DIAM = 7;

MOUNT_W = 10;
MOUNT_H = 25;
MOUNT_OFFSET = WALL_THICKNESS + 1;

USB_ACCESS = 50; // USB cable access x-position

ICON_SIZE = 10;

MP_BOARD_MOUNT = [62.7, 41.1, 3];
MP_BOARD_MOUNT_OFFSET = 8;
MP_BOARD_MOUNT_HOLE = 3; // M3 screw


rotate([180-TOP_ANGLE, 0, 0]) down(TOP_HEIGHT_FRONT) {
    difference() {
        color([0.4, 0.4, 0.4, 1.0]) 
            body();
        color([1.0, 1.0, 1.0, 1.0]) 
            symbols();
    }
}
rotate([180-TOP_ANGLE, 0, 0]) down(TOP_HEIGHT_FRONT) {
    color([1.0, 1.0, 1.0, 1.0]) 
        symbols();
}

back(10) {
    color([0.4, 0.4, 0.4, 1.0])
        base();
}

module body() {
    // Sides
    left((TOP_WIDTH-WALL_THICKNESS)/2) side_panel();
    right((TOP_WIDTH-WALL_THICKNESS)/2) side_panel(offset=-1);

    // Front
    cuboid([TOP_WIDTH, WALL_THICKNESS, TOP_HEIGHT_FRONT - CHAMFER], anchor=FRONT+BOTTOM, chamfer=CHAMFER, edges=[BOTTOM, LEFT, RIGHT]);

    // Back
    back(TOP_DEPTH-WALL_THICKNESS)
        difference() {
            cuboid([TOP_WIDTH, WALL_THICKNESS, TOP_HEIGHT_BACK-WALL_THICKNESS], anchor=FRONT+BOTTOM, chamfer=CHAMFER, edges=[BOTTOM, LEFT, RIGHT]);
            // Hole for USB cable
            translate([USB_ACCESS, epsilon, 1.6]) rotate([-90, 0, 0]) cylinder(h=WALL_THICKNESS+2*epsilon, d=3.2);
            translate([USB_ACCESS, epsilon, -epsilon]) cuboid([3.2, WALL_THICKNESS+2*epsilon, 1.6], anchor=FRONT+BOTTOM);
        }

    // Angled panel
    up(TOP_HEIGHT_FRONT) rotate([TOP_ANGLE, 0, 0]) down(WALL_THICKNESS)
    difference() {
        union() {
            cuboid([TOP_WIDTH, PANEL_DEPTH-WALL_THICKNESS*sin(TOP_ANGLE), WALL_THICKNESS], anchor=FRONT+BOTTOM, chamfer=CHAMFER, edges=[TOP, "Z"]);
            reinforcing();
        }
        cutouts();
    }

    // Mounting points - difference top panel
    difference() {
        up(MOUNT_OFFSET) {
            xs = [-(TOP_WIDTH/2 - WALL_THICKNESS - MOUNT_W/2), 0, TOP_WIDTH/2 - WALL_THICKNESS - MOUNT_W/2];
            ys = [MOUNT_W/2 + WALL_THICKNESS, TOP_DEPTH - MOUNT_W/2 - WALL_THICKNESS];
            for (x = xs) {
                translate([x, ys[0], 0])
                    mount(TOP_HEIGHT_FRONT);
                translate([x, ys[1], 0])
                    mount(TOP_HEIGHT_BACK-WALL_THICKNESS);
            }
        }
        up(TOP_HEIGHT_FRONT) rotate([TOP_ANGLE, 0, 0]) down(WALL_THICKNESS)
            cuboid([TOP_WIDTH, PANEL_DEPTH-WALL_THICKNESS*sin(TOP_ANGLE), WALL_THICKNESS*4], anchor=FRONT+BOTTOM, chamfer=CHAMFER, edges=TOP);
    }
}

module mount(h) {
    difference() {
        cuboid([MOUNT_W, MOUNT_W, h-MOUNT_OFFSET], anchor=BOTTOM);
        down(epsilon) m3_hole(h=MOUNT_H+2*epsilon); //cylinder(d=3, h=MOUNT_H+2*epsilon, anchor=BOTTOM);
    }
}

module side_panel(offset=1) {
    color("#CCCCCC") {
        cuboid([WALL_THICKNESS, TOP_DEPTH, TOP_HEIGHT_FRONT - WALL_THICKNESS*cos(TOP_ANGLE) + epsilon], anchor=FRONT+BOTTOM, chamfer=CHAMFER, edges=[BOTTOM, FWD, BACK]);
        up(TOP_HEIGHT_FRONT - WALL_THICKNESS*cos(TOP_ANGLE) + epsilon) back(TOP_DEPTH) rotate([0, 0, 180])
            difference() {
                wedge([WALL_THICKNESS, TOP_DEPTH, TOP_HEIGHT_BACK - TOP_HEIGHT_FRONT], anchor=FRONT+BOTTOM);
                translate([offset*WALL_THICKNESS/2, 0, 0]) 
                    chamfer_edge_mask(TOP_HEIGHT_BACK - TOP_HEIGHT_FRONT, chamfer=CHAMFER, anchor=BOTTOM);
            }
    }
}

module reinforcing() {
    SCREEN_BORDER_V = 10;
    SCREEN_BORDER_HL = 20;
    SCREEN_BORDER_HR = WALL_THICKNESS;
    back(SCREEN_DEPTH - SCREEN_BORDER_V) right(SCREEN_OFFSET)
        cuboid([SCREEN_W + SCREEN_BORDER_HL + SCREEN_BORDER_HR, SCREEN_H + 2*SCREEN_BORDER_V, REINF], anchor=FRONT+TOP, rounding=3, edges="Z");

    // Cutout for alpha dial
    translate([-TOP_WIDTH/SWITCH_SPREAD, SCREEN_DEPTH + SCREEN_H/2, 0])
            cylinder(d=DIAL_DIAM*3, h=REINF/2+2*epsilon, anchor=TOP);
}

module cutouts() {
    back(SCREEN_DEPTH) up(WALL_THICKNESS+epsilon) {
        // Screen front cutout
        right(SCREEN_OFFSET)
            cuboid([SCREEN_W, SCREEN_H, WALL_THICKNESS+REINF+2*epsilon], anchor=FRONT+TOP, rounding=2, edges="Z");

        back((SCREEN_H-SCREEN_BOARD.y)/2) down(1) right(SCREEN_BOARD_OFFSET + SCREEN_OFFSET) {
            // Screen board cutout
            cuboid(SCREEN_BOARD, anchor=FRONT+TOP, rounding=3, edges="Z");
            // Screen cable cutout
            back(SCREEN_BOARD.y/2) left(SCREEN_BOARD.x/2 - epsilon)
                cuboid([8, 23, SCREEN_BOARD.z*2], anchor=TOP+RIGHT, rounding=3, edges=LEFT);
        }

        // Switch cutouts
        for (x = [-1.5:1:1.5]) {
            translate([x * TOP_WIDTH/SWITCH_SPREAD, SCREEN_H + 20, 0]) {
                cylinder(d=SWITCH_DIAM, WALL_THICKNESS+REINF+2*epsilon, anchor=TOP);
                // Clearance for nuts
                down(WALL_THICKNESS)
                    cylinder(d=SWITCH_DIAM+12, REINF+2*epsilon, anchor=TOP);
                // Uncomment to check clearance for switch body
                //#cylinder(d1=15, d2=20, h=35, anchor=TOP);
            }
        }

        // Cutout for alpha dial
        translate([-TOP_WIDTH/SWITCH_SPREAD, SCREEN_H/2, 0])
             cylinder(d=DIAL_DIAM, h=REINF+WALL_THICKNESS+2*epsilon, anchor=TOP);
    }
}

module symbols() {
    up(TOP_HEIGHT_FRONT - LAYER*2) rotate([TOP_ANGLE, 0, 0]) {
        linear_extrude(LAYER*2 + epsilon) {
            xs = [-1.5, -0.5, 0.5, 1.5];
            icons = ["play_pause.svg", "list.svg", "star.svg", "skip.svg"];
            for (i = [0:3]) {
                translate([xs[i] * TOP_WIDTH/SWITCH_SPREAD - ICON_SIZE/2, SCREEN_H+50, 0]) {
                    import(icons[i], dpi=20);
                }
            }
        }
    }
}

/* BASE */

module base() {
    back(WALL_THICKNESS + BASE_OFFSET) {
        difference() {
            union() {
                cuboid(BASE, anchor=FRONT+BOTTOM, chamfer=CHAMFER, edges="Z");
                for (x = [-1, 0, 1]) {
                    for (y = [0, 1]) {
                        translate([x * (BASE.x/2 - MOUNT_W/2), MOUNT_W/2 + y * (BASE.y - MOUNT_W), 0])
                            base_mount();
                    }
                }
            }
            for (x = [-1, 0, 1]) {
                for (y = [0, 1]) {
                    translate([x * (BASE.x/2 - MOUNT_W/2 + BASE_OFFSET), MOUNT_W/2 + BASE_OFFSET + y * (BASE.y - MOUNT_W + BASE_OFFSET*2), 0])
                    base_mount_hole();
                }
            }
            // Hole for USB cable
            translate([USB_ACCESS, BASE.y, 0.2]) rotate([-10, 0, 0]) cuboid([3.2, WALL_THICKNESS*6, WALL_THICKNESS*2], anchor=BOTTOM + BACK);
        }
        back(BASE.y/2)
            back(MP_BOARD_MOUNT_OFFSET)
            for (x = [-1, 1]) {
                for (y = [-1, 1]) {
                    translate([x * MP_BOARD_MOUNT.x/2, y * MP_BOARD_MOUNT.y/2, WALL_THICKNESS])
                        mp_mount();
                }
            }
    }
}

module base_mount() {
    cuboid([MOUNT_W-BASE_OFFSET, MOUNT_W-BASE_OFFSET, MOUNT_OFFSET], anchor=BOTTOM, chamfer=CHAMFER, edges=[TOP, "Z"]);
}

module base_mount_hole() {
    down(epsilon) {
        cylinder(d=3.6, h=MOUNT_OFFSET+2*epsilon, anchor=BOTTOM);
        cylinder(d=6.5, h=WALL_THICKNESS+2*epsilon, anchor=BOTTOM);
    }
}

module mp_mount() {
    difference() {
        cyl(d=MP_BOARD_MOUNT_HOLE*2, h=MP_BOARD_MOUNT.z, anchor=BOTTOM, chamfer2=CHAMFER);
        m3_hole(MP_BOARD_MOUNT.z+2*epsilon);
    }
}

module m3_hole(h=1) {
    difference() {
        down(epsilon) cylinder(d=3, h=h, anchor=BOTTOM);
        for (t=[0, 120, 240]) {
            rotate([0, 0, t]) {
                translate([3/2+0.5, 0, 0])
                    cylinder(d=3/6+1, h=h, anchor=BOTTOM);
            }
        }
    }
}