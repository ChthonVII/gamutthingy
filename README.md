# gamutthingy
Converts .png images between color gamuts with chromatic adaptation and gamut compression mapping.

Principally intended for creating gamut conversion LUTs for FFNx and for color correcting texture assets for Final Fantasy 7 & 8.

Supercedes ntscjpng.

**Parameters:**
- `--help` or `-h`: Displays help.
- `--color` or `-c`: Specifies a single color to convert. A message containing the result will be printed to stdout. Should be a "0x" prefixed hexadecimal representation of a RGB8 color. For example: `0xFABF00`.
- `--infile` or `-i`: Specifies an input file. Should be a .png image.
- `--outfile` or `-o`: Specifies an input file. Should be a .png image.
- `--gamma` or `-g`: Specifies the gamma function to be assumed for the input. The inverse function will be applied to the output. Possible values are `srgb` (default) and `linear`. LUTs for FFNx should be created using linear RGB. Images should generally be converted using the sRGB gamma function.
- `--source-gamut` or `-s`: Specifies the source gamut. Possible values are:
     - `srgb`: The sRGB gamut used by (SDR) modern computer monitors. Identical to the bt709 gamut used for modern HD video.
     - `ntscjr`: The variant of the NTSC-J gamut used by Japanese CRT television sets. (whitepoint 9300K+27mpcd) Default.
     - `ntscj`: alias for `ntscjr`.
     - `ntscjb`: The variant of the NTSC-J gamut used for SD Japanese television broadcasts. (whitepoint 9300K+8mpcd)
     - `smptec`: The SMPTE-C gamut used for American CRT television sets/broadcasts and the bt601 video standard.
     - `ebu`: The EBU gamut used in the European 470bg television/video standards (PAL).
- `--dest-gamut` or `-d`: Specifies the destination gamut. Possible values are the same as for source gamut. Default is `srgb`.
- `--map-mode` or `-m`: Specifies gamut mapping mode. Possible values are:
     - `clip`: Linear RGB output is simply clipped to 0, 1. Detail in the out-of-bounds range will be lost.
     - `compress`: Uses a gamut (compression) mapping algorithm to remap out-of-bounds colors to a smaller zone inside the gamut boundary. Also remaps colors originally in that zone to make room. Essentially trades away some colorimetric fidelity in exchange for preserving some of the out-of-bounds detail. Default.
     - `expand`: Same as `compress` but also applies the inverse of the compression function in directions where the destination gamut boundary exceeds the source gamut boundary. Also, reverses the order of the steps in the `vp` algorithm. The only use for this is to prepare an image for a "roundtrip" conversion. For example, if you want to display a sRGB image as-is in FFNx's NTSC-J mode, you would convert from sRGB to NTSC-J using `expand` in preparation for FFNx doing the inverse operation.
- `--gamut-mapping-algorithm` or `--gma`: Specifies which gamut mapping algorithm to use. (Does nothing if `--map-mode clip`.) Possible values are:
     - `cusp`: Slightly modified version of the CUSP algorithm decribed in [1]. Modifications discussed below.
     - `hlpcm`: Slightly modified version of the HLPCM algorithm described in [2]. Modifications discussed below.
     - `vp`: Slightly modified version of the VP algorithm described in [3]. Modifications discussed below. Default.
- `--safe-zone-type` or `-z`: Specifies how the outer zone subject to remapping and the inner "safe zone" exempt from remapping are defined. Possible values are:
     - `const-fidelity`: The standard approach in which the zones are defined relative to the distance from the "center of gravity" to the destination gamut boundary. Default.
     - `const-detail`: The approach described in [4] in which the remapping zone is defined relative to the difference between the distances from the "center of gravity" to the source and destination gamut boundaries. As implemented here, an overriding minimum size for the "safe zone" (relative to the destination gamut boundary) may also be enforced. 
- `--remap-factor` or `--rf`: Specifies the size of the remapping zone relative to the difference between the distances from the "center of gravity" to the source and destination gamut boundaries. (Does nothing if `--safe-zone-type const-fidelity`.) Default 0.4.
- `--remap-limit` or `--rl`: Specifies the size of the safe zone (exempt from remapping) relative to the distance from the "center of gravity" to the destination gamut boundary. If `--safe-zone-type const-detail`, this serves as a minimum size limit when application of `--remap-factor` would lead to a smaller safe zone. Default 0.9.
- `--knee` or `-k`: Specifies the type of knee function used for compression, `hard` or `soft`. Default `soft`.
- `--knee-factor` or `--kf`: Specifies the width of the soft knee relative to the size of the remapping zone. (Does nothing if `--knee hard`.) Note that the soft knee is centered at the knee point, so half the width extends into the safe zone, thus expanding the area that is remapped. Default 0.4.
- `--dither` or `--di`: Specifies whether to apply dithering to the ouput, `true` or `false`. Uses Martin Roberts's quasirandom dithering algorithm described in [5]. Dithering should not be used for LUTs. Dithering should be used for images in general. Default `true`.
- `--verbosity` or `-v`: Specify verbosity level. Integers 0-5. Default 2.

**General Usage:**
- To prepare a LUT compatible with FFNx: Process the supplied neutral LUT, 64.png, using selected parameters. Gamma should linear and dithering should be off. For example: `gamutthingy -i 64.png -o lut.png --remap-limit 0.9 --knee soft --safe-zone-type const-fidelity --knee-factor 0.4 --gma vp --di false --map-mode compress -g linear -s ntscj -d srgb`
- To prepare a sRGB image for use in FFNx's NTSC-J mode: Convert from sRGB to NTSC-J with `--map-mode expand`. The LUTs that ship with FFNx were made with the default settings, so use the defaults unless you've replaced the LUTs. For example: `gamutthingy -i input.png -o output.png --remap-limit 0.9 --knee soft --safe-zone-type const-fidelity --knee-factor 0.4 --gma vp --di true --map-mode expand -g srgb -s srgb -d ntscj`
- To prepare a NTSC-J image for use in FFNx's sRGB mode: Convert from NTSC-J to sRGB with `--map-mode expand`. The LUTs that ship with FFNx were made with the default settings, so use the defaults unless you've replaced the LUTs. For example: `gamutthingy -i input.png -o output.png --remap-limit 0.9 --knee soft --safe-zone-type const-fidelity --knee-factor 0.4 --gma vp --di true --map-mode expand -g srgb -s ntscj -d srgb`

**Implementation Details:**
- When converting between gamuts with different white points, chromatic adaptation is done via the "Bradford method" described in [6]. See also [7].
- Gamut mapping operations are done in the JzCzhz colorspace, the polar cousin to Jzazbz, described in [8]. A couple notes on JzCzhz:
     - Scaling the units of the XYZ input to set the absolute brightness causes the hue angles to rotate. Most of this rotation happens at very low brightness. For example, linear RGB red (1,0,0) rotates about 10 degrees going from 1 nit to 100 nits, but only about 1 degree going from 100 nits to 10,000 nits. I don't know if this is just a flaw in Jzazbz's design or an accurate depiction of some brightness-hue interaction like the Bezold–Brücke shift, or something else entirely. In part to avoid any problems here, everything is scaled to 200 nits.
     - The inverse PQ function used in Jzazbz -> XYZ conversion can sometimes produce NAN outputs. Without doing a formal analysis and proof, I *assume* this is *always* the result of asking pow() to do something that leads to an imaginary or complex number, and *only* happens on inputs that fall outside any possible gamut.
- Gamut boundaries are sampled as described in [9]. Hue is sampled in 0.2 degree slices. For the coarse sampling phase, luminance is sampled in 20 steps from black to white, and chroma is sampled in 50 steps from neutral to 110% of the highest chroma value found among the red, green, and blue points. For the fine sampling phase, both luminance and chroma are sampled in 50 steps between coarse samples. To find the boundary in a given direction, the boundary in each of the adjacent sampled hue slices is found via 2D line-line intersection, then extrapolated for the input hue via 3D line-plane intersection. To find the luminance of the cusp at a given hue for the CUSP gamut mapping algorithm, the weighted average of the cusp luminance of the adjacent sampled hue slices is used.
- Three gamut mapping algorithms are available:
    - CUSP, described in [1].
    - Hue Lightness Preserving Chroma Mapping (HLPCM), described in [2]. (Note that this algorithm has been around under various names since at least the 1990s.)
    - Vividness-Preserved (VP), described in [3].
- The gamut mapping algorithms have been modified as follows:
     - Linear lightness scaling is always used. (This is why CUSP is used instead of GCUSP or SGCK.) There are four reasons for this: First, we simply don't have a good values for the typical brightness of the devices in our core use cases. Consumer television and PC monitor manufacturers generally ignore the specs and seek to offer as much brightness as possible. Thus maximum brightness varies between manufacturers and models, and is very rarely documented. (For instance, [wikipedia's article on comparing display technologies](https://en.wikipedia.org/wiki/Comparison_of_CRT,_LCD,_plasma,_and_OLED_displays) cites only one single test of one single CRT television.) Second, to the extent we have to make a guess, the maximum brightness of late-90s CRT televisions, modern SDR monitors, and modern HDR monitors in SDR mode are all likely fairly close to the same -- around 200 nits. Third, to the extent we have to make a guess, late-90s CRT televisions were probably slightly dimmer than modern monitors, leading to the situation for our main use case (NTSC-J and sRGB) where the dimmer gamut is the wider one. This is the reverse of what's expected in all the literature, and tends to undermine the rationales for various lightness scaling regimes. Fourth, I'm uneasy about working in Jzazbz on inputs of differing max lightness. (See above.) For all these reasons, lightness is simply scaled to 200 nits for the D65 white point.
     - The compression function is tunable. It's possible to move the knee point, use a hard or soft knee with a tunable width, or to use the unique approach to zone definition from [4]. Typically the outer zone subject to remapping and the inner "safe zone" exempt from remapping are defined relative to the distance from the "center of gravity" to the destination gamut boundary, but [4] defines them relative to the difference between the distances from the "center of gravity" to the source and destination gamut boundaries. In this mode, an overriding minimum size for the "safe zone" can also be set. (While intriguing, I have not been able to find parameters for which [4]'s approach works as well as the typical one. A soft knee seems intuitively superior to a hard knee, but it often makes little difference in practice when quantizing to RGB8 in the end anyway.)
     - VP has been altered to fix an obvious flaw. VP is a three-step algorithm: lightness compression (which is replaced with linear scaling as per above), compression towards black, and finally compression towards neutral. There is a problem with the third step. If this step is applied across the entire lightness range, some above-the-cusp colors are unnecessarily desaturated. Colors in the outer 10% remapping zone are pulled back from the boundary, but there are no out-of-bounds colors to fill in the space thus vacated (because step two pulled them all below the cusp). On the other hand, if step three is applied only to below-the-cusp colors, it causes a discontinuity at the cusp's luminosity where funny stuff happens, including inversion of relative chroma between formerly adjacent colors. The fix for step three employed here is as follows: Define a working gamut boundary by ignoring the above-the-cusp boundary segments and extrapolating the just-below-the-cusp segment infinitely upward. (The mirror of what was done for step two.) Map colors back from this boundary *parallel* to the just-above-the-cusp boundary segment. (The one that was extrapolated in step two.) This avoids unnecessarily emptying the outer 10% zone above the cusp and also avoids creating a discontinuity. (It should be noted that this fix isn't quite proper. A proper fix would move colors along a warped path that tracks parallel to the upper side of the gamut boundary, rather than a straight path parallel to only its final segment. But we can generally "get away with it" because the upper side of the gamut boundary should be very close to straight for the short distances typically used for remapping zones.)
- Dithering is done using Martin Roberts's quasirandom dithering algorithm described in [5].
- PNG plumbing shamelessly borrowed from png2png example by John Cunningham Bowler.

**References:**
- [1] Morovic, Ján. "To Develop a Universal Gamut Mapping Algorithm." Ph.D. Thesis. University of Derby, October 1998. ([Link](https://ethos.bl.uk/OrderDetails.do?did=1&uin=uk.bl.ethos.302487))
- [2] Addari, Gianmarco. "Colour Gamut Mapping for Ultra-HD TV." Master's Thesis. University of Surrey, August 2016. ([Link](https://www.insync.tv/wp-content/uploads/2019/12/Report_on_colour_gamut_mapping.pdf))
- [3] Xu, Liaho, Zhao, Baiyue, & Luo, Ming Ronnier "Colour gamut mapping between small and large colour gamuts: Part I. gamut compression." *Optics Express*, Vol. 26, No. 9, pp. 11481-11495. April 2018. ([Link](https://opg.optica.org/oe/fulltext.cfm?uri=oe-26-9-11481&id=385750))
- [4] Su, Chang, Tao, Li, & Kim, Yeong Taeg. "Color-gamut mapping in the non-uniform CIE-1931 space with perceptual hue fidelity constraints for SMPTE ST.2094-40 standard." *APSIPA Transactions on Signal and Information Processing*, Vol. 9, E. 12. March 2020. ([Link](https://www.cambridge.org/core/journals/apsipa-transactions-on-signal-and-information-processing/article/colorgamut-mapping-in-the-nonuniform-cie1931-space-with-perceptual-hue-fidelity-constraints-for-smpte-st209440-standard/D2E1B9B7E0D5FCA1A5722D24F5F435A3))
- [5] Roberts, Martin. "The Unreasonable Effectiveness of Quasirandom Sequences." *Extreme Learning*. April 2018. ([Link](https://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/))
- [6] Lam, K.M. "Metamerism and Colour Constancy." Ph.D. Thesis. University of Bradford, 1985.
- [7] Lindbloom, Bruce. "Chromatic Adaptation." BruceLindbloom.com. April 2017. ([Link](http://www.brucelindbloom.com/index.html?Eqn_ChromAdapt.html))
- [8] Safdar, Muhammad, Cui, Guihua, Kim, You Jin, & Luo, Ming Ronnier. "Perceptually uniform color space for image signals including high dynamic range and wide gamut." *Optics Express*, Vol. 25, No. 13, pp. 15131-15151. June 2017. ([Link](https://opg.optica.org/fulltext.cfm?rwjcode=oe&uri=oe-25-13-15131&id=368272))
- [9] Lihao, Xu, Chunzhi, Xu, & Luo, Ming Ronnier. "Accurate gamut boundary descriptor for displays." *Optics Express*, Vol. 30, No. 2, pp. 1615-1626. January 2022. ([Link](https://opg.optica.org/fulltext.cfm?rwjcode=oe&uri=oe-30-2-1615&id=466694))

**Building:**

Linux:
- Install libpng-dev >= 1.6.0
- `make`

Windows:
- Either install libpng >= 1.6.0 where your linker knows to find it, or edit the #includes to use a local copy.
- TODO make a visual studio project file

