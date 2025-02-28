**Summary:**

"Minimum perceptible color difference" or "MPCD" is a unit of distance in an obsolete uniform color space in a direction perpendicular to the Plankian (blackbody) locus at a given point. The positive MPCD direction goes away from the Plankian locus on the convex side, generally towards green. See [1], [2], and [3]. There are two different units named "MPCD." The more common, and more recent, usage is 0.0004 delta-uv in the CIE1960 Uniform Color Space. See [1] and [2]. The older, nearly forgotten usage is 0.0005 delta-uv in the Judd1935 Uniform Color Scale (or, rather, to be precise, in MacAdam's xy-to-uv transformation that is (nearly) equivalent to Judd's UCS). See [3] and [4]. I'm going to call these "CIE MPCDs" and "Judd MPCDs." 

**About CIE MPCDs:**

 The equations to convert from CIE1931 xy coordinates to CIE1960 UCS and back again are simple and well-known See [link](https://colour.readthedocs.io/en/v0.3.16_b/_modules/colour/models/cie_ucs.html#UCS_uv_to_xy).

- `u = 4x / (12y - 2x + 3)`
- `v = 6y /  (12y - 2x + 3)`
- `x = 3u / (2u - 8v +4)`
- `y = 2v / (2u - 8v +4)`

VESA FPDM ([1]) says that a MPCD is defined as 0.004 delta-uv. However, I am 99.9% certain that this must be a typo and that 0.0004 is the correct value. A value of 0.004 is simply too big to be correct. A shift of 0.004 delta-uv results in a flagrantly obvious color difference, rather than a minimally perceptible one, and shifting by a small multiple of 0.004 flies far across the chromaticity diagram. By contrast, 0.0004 gives very plausible results. See below.

**About Judd MPCDs:**

My conclusion here required more historical guesswork. We have two sources that mention MPCD in the Judd UCS, [3] and [4]. Both of these cite to [5]. However, [5] describes a trilinear coordinate space rather than a Cartesian one. (And the trilinear distance from 9300K to x = 0.281, y = 0.311 is nowhere near 27 * 0.0005, as [3] and [4], taken together, tell us it should be.) The inquiry is complicated here by the fact that there are three competing conversions from Judd's UCS to Cartesian coordinates, and it's not clear which one [3] and [4] are assuming (or even if they're assuming the same one).
1. Judd gave us a simple transformation in the appendix to [5]: x = (2b + g) / sqrt3; y = g, where b and g are two of the trilinear coordinates.
2. The xy-to-uv transformation provided in [6] that is equivalent to the Judd UCS. (However, I suspect that this equation was well known prior to 1944. Rather than an article on new research, [6] is a preview of a committee report chapter summarizing techniques that seem to have been well known in the field at the time. MacAdam explained the math behind [6] in [7] back in 1937. However, for his example in [7], he used a simplified version that latter became the CIE1960 UCS rather than the more complex equations shown in [6]. According to [8], Judd and MacAdam were both on the committee listed as the author of [6].)
3. A different xy-to-uv transformation, equivalent to the Judd UCS, provided in [9]. Also based on the math in [7].

I am reasonably certain that [3] and [4] were assuming the xy-to-uv transformation provided in [6], for the following reasons:
- Taken together, [3] and [4] tell us that the delta-uv distance from 9300K on the Plankian locus to x = 0.281, y = 0.311 should be 27 * 0.0005. Using the xy-to-uv transformation provided in [6] comes plausibly close to that (27 * ~0.00051), and slightly closer than the other possibilities.
- As noted above, [6] is a preview of a committee report chapter summarizing techniques that seem to have been well known in the field at the time, suggesting wide adoption.
- Latter-day sources, like [wikipedia](https://en.wikipedia.org/wiki/CIE_1960_color_space#Background), conflate the xy-to-uv transformation provided in [6] with the Judd UCS, suggesting wide adoption. Also, if the wikipedia authors conflate [6] with [5], maybe the authors of [3] and [4] did too.
- The xy-to-uv transformation in [9] was dubbed "RUCS" by its authors. If the authors of [3] and [4] were using RUCS, presumably they would have called it by name.

The CIE1931 xy to uv equations in [6] are:
```
a = 0.4661;
b = 0.1593;
c = -0.15735;
d = 0.2424;
e = 0.6581;
u = (ax + by) / (y + cx +d)
v= ey / (y + cx +d)
```

The reverse transform is not provided in [6]. Solving by substitution, I get:
```
a = 0.4661;
b = 0.1593;
c = -0.15735;
d = 0.2424;
e = 0.6581;
y = dv / (e - v - ((c * (eu - bv)) / a))
x = y * ((eu - bv) / av)
```

These are ugly, and I hope someone with better algebra skills than I can simplify them into something prettier.

[3] tells us clearly that a Judd MPCD is 0.0005 units. (The hard question was, " In which Cartesian equivalent of the Judd UCS?")

**Test Results**

I [implemented these equations in gamutthingy](https://github.com/ChthonVII/gamutthingy/blob/master/src/colormisc.cpp#L366). It gives the following results:
- For 9300K + 8 CIE MPCD, I get x=0.283013, y=0.297166. This is remarkably close to x=0.2831, y=0.2971 stated in  [10].
- For 9300K + 8 Judd MPCD, I get x=0.283752, y=0.298031. This is reasonably close to x=0.2838, y=0.2984 stated in [11].
- The slight difference between CIE and Judd MPCD units probably accounts for the conflict between [10] and [11]. The authors were using different units!
- For 9300K + 27 Judd MPCD, I get x=0.281103, y=0.309955. This is reasonably close to x=0.281, y=0.311 stated in [4].

I believe the small differences between my results and the coordinates stated in the literature can be attributed to authors of these papers, working in the 1970s, relying on tables of rounded/truncated values for their calculations, whereas I today have a digital computer that uses 64 bits to represent each floating point value.

**Sources**

1. Charles Poynton, Digital Video and HD: Algorithms and Interfaces, 2nd Edition, pp. 277-78.
2. VESA Flat Panel Display Measurements Standard Version 2.0, Appendix 201.
3. B. R. Canty & G. P. Kirkpatrick, Color Temperature Diagram, J. Opt. Soc. Am., Vol. 51, No. 10, p. 1130 (Oct. 1961).
4. Yoshinobu Nayatani, Shoichi Hitani, Kyosuke Furukawa, Yutaka Kurioka and Isamu Ueda, Development of the White-Standard Apparatus for Color Cathode-ray Tubes , Television, Vol. 24, No. 2, p. 116 (1970). Machine translation of the relevant passage:

> The Electronics and Mechanical Engineering Association requested that the white standard to be fabricated here have chromaticity coordinates x = 0.281, y = 0.311 (color temperature 9300°K + 27 MPCD) and have the characteristics of a luminance standard for color picture tubes. The chromaticity coordinates are specified on the Uniform Chromaticity Scale (UCS) of Judd) (where C2 = 1.4380) so that the deviation from the blackbody radiation locus is +27 MPCD at a color temperature of 9300°K. The color temperature of a light source is defined as the temperature of the black body radiator that is closest to the chromaticity coordinates of the light source, considering the locus of the black body radiator at 9300°K on the UCS chromaticity diagram. Therefore, if the chromaticity diagram is different, for example on the 1960 CIEUCS chromaticity diagram10), the color temperature and MPCD of the light source will also be different. However, in accordance with the US standard JEDEC4), the design is carried out with this chromaticity coordinate as the target value. It was decided to carry out the following.

5. Deane B. Judd, A Maxwell Triangle Yielding Uniform Chromaticity Scales,  J. Opt. Soc. Am., Vol. 25, p. 24 (Jan 1935).
6. Committee on Colorimetry, Quantitative Data and Methods for Colorimetry,  J. Opt. Soc. Am., Vol. 34, No. 11, p. 633 (Nov. 1944).
7. David L. MacAdam, Projective Transformations of I. C. I. Color Specifications,  J. Opt. Soc. Am., Vol. 27, p. 294 (Aug 1937).
8. Sean F. Johnson, The Construction of Colorimetry by Committee, Science in Context, Vol. 9, No. 4,  p. 387 (1996).
9. F. C. Breckenridge & W. R. Schaub, Rectangular Uniform-Chromaticity-Scale Coordinates, J. Opt. Soc. Am., Vol. 29, p. 370 (Sep 1939).
10. Yoshitomi Nagaoka, On the Color Reproduction and Reference White of Color Television, Journal of the Television Society, Vol. 33, No. 12, p. 1013 (1979).
11. Shigeru Yagishita, Kenji Nishino, Katsuhiro Ohta, and Takashi Ishii, Color Picture Tube with Built in Reference White for Color Master Monitors), Television, Vol. 31, No. 11, p. 883 (1977).
