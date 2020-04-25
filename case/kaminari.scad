/*
 * Kaminari
 *
 * Copyright (C) 2020 Richard "Shred" Körber
 *   https://github.com/shred/kaminari
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

basesize = 60;
tipheight = 40;
wall = 1.5;

mount_distance = 40;
mount_diameter = 7;
mount_screw_diameter = 3;
mount_screw_length = 10;
mount_head_depth = 2;
mount_head_diam = 5.8;

usb_width = 11;
usb_height = 7;

module pyramid(size) {
    height = size/2 * sqrt(3);
    polyhedron(
        points=[
            [ size/2,  size/2, 0     ],
            [ size/2, -size/2, 0     ],
            [-size/2, -size/2, 0     ],
            [-size/2,  size/2, 0     ],
            [      0,       0, height]
        ],
        faces=[
            [0,1,4],
            [1,2,4],
            [2,3,4],
            [3,0,4],
            [1,0,3], [2,1,3]
        ]
    );
}

module led() {
    color("silver") cylinder(d=10, h=1.3);
    color("white") translate([0, 0, 1.3 + (2.8-1.3)/2]) cube([5, 5, 2.8-1.3], center=true);
}

module detector() {
    translate([-28.4/2, -30.1/2, 0]) {
        color("purple") difference() {
            linear_extrude(height=2.4) {
                polygon([[0, 0], [28.4, 0], [28.4, 9.2], [20.25, 15.1], [20.25, 30.1], [8.15, 30.1], [8.15, 15.1], [0, 9.2], [0, 0]]);
            }
            translate([ 8.15 + 3/2 + 1, 21, -1]) cylinder(d=2.95, h=4);
            translate([20.25 - 3/2 - 1, 21, -1]) cylinder(d=2.95, h=4);
        }
        color("darkslategrey") translate([8.15, 30.1-3.2, 2.4]) cube([12.1, 3.2, 1.8]);
        color("black") translate([0, 0, -1]) cube([28.4, 2.5, 11.4]);
    }
}

module mcu() {
    translate([-25.8/2, -34/2, 8]) {
        color("royalblue") cube([25.8, 34, 1.3]);
        color("silver") translate([(25.8-12)/2, 12, 1.3]) cube([12, 15, 3.2]);
        color("black") translate([0, 0, -2]) cube([25.8, 28, 2]);
        color("silver") translate([(25.8-7.8)/2, -0.1, -1.9]) cube([7.8, 7, 1.9]);
        color("black") translate([0, 7.4, -8]) cube([2.5, 21, 8]);
        color("black") translate([25.8-2.5, 7.4, -8]) cube([2.5, 21, 8]);
    }
}

module stripe(dim) {
    translate([dim[1]/2, 0, 0]) cylinder(d=dim[1], h=dim[2]);
    translate([dim[1]/2, -dim[1]/2, 0]) cube([dim[0] - dim[1], dim[1], dim[2]]);
    translate([dim[0] - dim[1]/2, 0, 0]) cylinder(d=dim[1], h=dim[2]);
}

module mountBar(height) {
    difference() {
        cylinder(d = mount_diameter, h = height);
        cylinder(d = mount_screw_diameter, h = mount_screw_length, $fn = 5);
    }
}

module upper() {
    difference() {
        intersection() {
            pyramid(basesize);
            ph =  basesize/2 * sqrt(3);
            translate([0, 0, ph/2 - 1]) cube([basesize, basesize, ph], center=true);
        }
        pyramid(basesize - wall*2);
        translate([(-usb_width)/2, (usb_height-7)/2-20, 8]) rotate([90, 0, 0]) stripe([usb_width, usb_height, 10]);
    }

    intersection() {
        union() {
            translate([ mount_distance/2,  mount_distance/2, wall*2]) mountBar(basesize);
            translate([ mount_distance/2, -mount_distance/2, wall*2]) mountBar(basesize);
            translate([-mount_distance/2,  mount_distance/2, wall*2]) mountBar(basesize);
            translate([-mount_distance/2, -mount_distance/2, wall*2]) mountBar(basesize);
        }
        pyramid(basesize);
    }
}

module middle_led() {
    difference() {
        translate([-10, -10, 0]) cube([20, 20, 3.5]);
        translate([0, 0, 1]) cylinder(d=10.5, h=4);
        rotate([0, 0, -180]) translate([-4.3, -2, -.1]) stripe([8.6, 5, 3.2]);
    }
}

module lower_detector() {
    translate([-11.5, -basesize/2 - 15, 0]) cube([4, basesize, 20]);
}

module middle() {
    intersection() {
        difference() {
            union() {
                // LED holder
                translate([0, 0, 35]) middle_led();
                translate([-basesize/2, -basesize/2, 22]) cube([basesize, 30, 13]);
            }
            translate([ 3.5, -6, 21.9]) cylinder(d=3, h=12, $fn=5);
            translate([-3.5, -6, 21.9]) cylinder(d=3, h=12, $fn=5);
        }
        pyramid(basesize - wall*2);
    }
}

module basePlate() {
    difference() {
        union() {
            translate([0, 0, wall/2]) cube([basesize, basesize, wall], center=true);
            translate([ mount_distance/2,  mount_distance/2, 0]) cylinder(d = mount_diameter, h = wall*2);
            translate([ mount_distance/2, -mount_distance/2, 0]) cylinder(d = mount_diameter, h = wall*2);
            translate([-mount_distance/2,  mount_distance/2, 0]) cylinder(d = mount_diameter, h = wall*2);
            translate([-mount_distance/2, -mount_distance/2, 0]) cylinder(d = mount_diameter, h = wall*2);
        }

        translate([ mount_distance/2,  mount_distance/2, 0]) cylinder(d = mount_screw_diameter, h = mount_screw_length);
        translate([ mount_distance/2, -mount_distance/2, 0]) cylinder(d = mount_screw_diameter, h = mount_screw_length);
        translate([-mount_distance/2,  mount_distance/2, 0]) cylinder(d = mount_screw_diameter, h = mount_screw_length);
        translate([-mount_distance/2, -mount_distance/2, 0]) cylinder(d = mount_screw_diameter, h = mount_screw_length);

        translate([ mount_distance/2,  mount_distance/2, 0]) cylinder(d1 = mount_head_diam, d2 = mount_screw_diameter, h = mount_head_depth);
        translate([ mount_distance/2, -mount_distance/2, 0]) cylinder(d1 = mount_head_diam, d2 = mount_screw_diameter, h = mount_head_depth);
        translate([-mount_distance/2,  mount_distance/2, 0]) cylinder(d1 = mount_head_diam, d2 = mount_screw_diameter, h = mount_head_depth);
        translate([-mount_distance/2, -mount_distance/2, 0]) cylinder(d1 = mount_head_diam, d2 = mount_screw_diameter, h = mount_head_depth);

        for (x = [-8 : 2 : 8]) {
            translate([x, 0, wall/2-1]) cube([1, 25, wall+3], center=true);
        }
    }
}

module lower() {
    intersection() {
        union() {
            basePlate();
            translate([-16, -basesize, 0]) cube([3, basesize, 22]);
            translate([16-3, -basesize, 0]) cube([3, basesize, 22]);

            difference() {
                union() {
                    translate([-13.5, -28, 0]) cube([27, 10, 5.7]);
                    translate([-13.5, -26.2, 0]) cube([27, 3, 13]);
                }
                translate([(-usb_width)/2, (usb_height-7)/2-23.19, 8]) rotate([90, 0, 0]) stripe([usb_width, usb_height, 10]);
            }

            difference() {
                union() {
                    translate([12.5, 11, 3]) cube([7, 10, 8], center=true);
                    translate([-12.5, 11, 3]) cube([7, 10, 8], center=true);
                }
                translate([0, -6.1, 7]) cube([25.8, 34, 2.6], center=true);
            }
        }
        pyramid(basesize - wall*2);
    }
}

module middle2() {
    intersection() {
        difference() {
            union() {
                translate([0, 9, 13.3]) cube([32, 6, 12.6], center=true);
                translate([-16, 0, wall]) cube([3, 6, 19.6-wall]);
                translate([16-3, 0, wall]) cube([3, 6, 19.6-wall]);
                translate([-16, 0, wall]) cube([3, 4, 30-wall]);
                translate([16-3, 0, wall]) cube([3, 4, 30-wall]);
            }
            translate([0, 9, 20]) cube([14, 7, 6], center=true);
            translate([0, 9, 10]) cube([18, 7, 6], center=true);
        }
        pyramid(basesize - wall*2);
    }
}

if (!$preview) {
    upper();
}
middle();
middle2();
lower();

translate([0, 0, 22]) rotate([0, 180, 180]) detector();
translate([0, -6.2, 15]) rotate([0, 180, 0]) mcu();
translate([0, 0, 36]) led();

$fn=90;
