# gamutthingy


**NOTE:** This readme file has been undergoing substantial revision. At this point, it's nearly done. Many citations are missing, but the content is pretty much finished.

Performs color gamut conversion conversion with chromatic adaptation and gamut compression mapping, and optionally simulates the "color correction" behavior of several CRT televisions.

Principally intended for adjusting the colors of CRT-era games for modern sRGB displays.

Four general modes of operation:
- Convert a single color input and print the result to the console.
- Convert a .png image input and save the output to a .png image.
- Generate a 3D lookup table (LUT) and save the output to a .png image.
- Simulate the color palette of a NES/Famicom and save the output to a .pal file in the format used by most NES emulators.

#### Parameters
**Input Modes:**
- `--color` or `-c`: Specifies a single color to convert. Should be a "0x" prefixed hexadecimal representation of an RGB8 color. For example: `0xFABF00`.  A message containing the result will be printed to stdout.
- `--infile` or `-i`: Specifies an input file to convert. Should be a .png image. Output will be saved to the output file specified with `-o` or `--outfile`.
- `--lutgen`: Generate a LUT. Possible values are `true` or `false`(default). Output will be saved to the output file specified with `-o` or `--outfile`.
- `--nespalgen`: Generate a NES/Famicom palette. Possible values are `true` or `false`(default). Output will be saved to the output file specified with `-o` or `--outfile`.

**Input-Related Parameters:**
- `--backwards` or `-b`: Enables backwards search mode. Possible values are `true` or `false`(default). In backwards search mode, the user-supplied input is treated as the desired output and gamutthingy searches for an input that yields that output (or as close as possible). This is equivalent to performing the inverse of the specified operations. This is useful for roundtrip conversions and two-step conversions. If backward search mode is enabled and `--lutmode postcc`, then `--crtclamplow` and `--crtclamphigh` will be forced to 0.0 and 1.0. Backwards search mode "works" with NES palette generation, but it's hard to imagine the output being of any use. WARNING: Backwards search mode can be VERY SLOW. (Alternatively, `--map-mode expand` also performs inverse operations. However backwards search mode is preferred because (1) backward search mode *guarantees* the closest possible match after RGB8 quantization, while `--map-mode expand` merely assumes its inverse functions will quantize to best matches, and (2) backwards search mode works in combination with CRT simulation, while `--map-mode expand` generally does not.)
- `--gamma-in` or `--gin`: Specifies the gamma function to be applied to the input. Possible values are `srgb` (default), `linear`, and `rec2084`. Will be ignored if CRT simulation before gamut conversion is enabled (`--crtemu front`) since the CRT EOTF function will be used instead. (Note that `rec2084` is not very useful since 16-bit png input isn't supported yet.)
- `--hdr-sdr-max-nits` or `--hsmn`: See same in "Output Parameters," below.
- `--lutsize`: Specifies the size of the LUT to generate. E.g., `--lutsize 64` will result in a 64x64x64 LUT. Integer number. Default 128.
- `--lutmode` or `--lm`: Specifies the LUT type. Possible values are `normal` (default), `postcc`, and `postgamma`. When not simulating a CRT television prior to gamut conversion (i.e., not `--crtemu front`), then `--lutmode` is forced to `normal` and the color at each index is treated as gamma-space R'G'B' or linear RGB according to `--gamma-in`. The behavior when simulating a CRT television prior to gamut conversion is described below: 
     - `normal` The color at each index is treated as gamma-space R'G'B' before it is encoded to composite and sent to the CRT television. All CRT properties (color correction, hue, clamping, EOTF, etc.) will be baked into the LUT. Any setting is permissible for CRT clamping (`--crtclamplow`, `--crtclamphigh`, and `--crtclamphighenable`), including `--crtclamphighenable false`. In order to correctly compute the relative distance from an input color to the 8 nearest indices for interpolation, the calling code must simulate the CRT behavior, up to and including the EOTF function, on the input and the 8 indices. However, for a large enough LUT, just computing the EOTF will probably yield the correct result when quantizing to RGB8. This mode is the only option for simulating a CRT with no clamping, and preferable for simulating a CRT clamped to  a wide range.
     - `postcc` The color at each index is treated as gamma-space R'G'B' after it has been demodulated/color corrected and clamped, and then shifted and scaled to fit in the range 0-1. Clamping is necessary for this mode, and `--crtclamphighenable` will be forced to `true`. Additionally, clamping to a relatively narrow range is recommended, as out-of-bounds values will otherwise consume too much of the LUT's bandwidth. If backwards search mode is enabled, clamping will be forced to 0-1. The calling code is expected to perform the demodulation/color correction, hue adjustment, clamping, and shifting and scaling to 0-1, while the EOTF function will be baked into the LUT. In order for the source gamut pruning assumed by the LUT to remain valid, the calling code must use the **same** demodulation, etc. as was used to generate the LUT. In order to correctly compute the relative distance from an input color to the 8 nearest indices for interpolation, the calling code must apply the EOTF function to the input and the 8 indices. This mode may be preferable when you want both the freedom to simulate CRT/composite artifacts beyond the scope of gamutthingy and also some extent of out-of-bounds values handled by gamut compression rather than clamping. (This functionality was previously named `--eilut`.)
     - `postgamma` The color at each index is treated as linear RGB after the entire CRT simulation. CRT clamping will be forced to 0-1. The calling code is expected to perform the entire CRT simulation. In order for the source gamut pruning assumed by the LUT to remain valid, the calling code must use the **same** simulation as was used to generate the LUT. This mode is preferable if you want the freedom to simulate CRT/composite artifacts beyond the scope of gamutthingy, and do not mind strictly clamping the demodulation output. It may also be preferable if you want precise interpolation, and do not mind strictly clamping the demodulation output.
- `--nespalmode`: When generating NES palette, simulate a PAL NES model instead of a NTSC one. Possible values are `true` or `false`(default).
- `--nesburstnorm`: When generating NES palette, normalize chroma to the amplitude of the NES's colorburst, rather than assuming 40 IRE. While CRT televisions should do this, in practice some did and some didn't. Possible values are `true`(default) or `false`.
- `--nesskew26a`: When generating NES palette, phase skew, in degrees, caused by design of the traces for hues \#2, \#6, and \#A. Floating point number. Default 4.5.
- `--nesboost48c`: When generating NES palette, luma boost, in IRE units, caused by design of the traces for hues \#4, \#8, and \#C. Floating point number. Default 1.0.
- `--nesperlumaskew`: When generating NES palette, phase skew, in degrees, for each step of increasing luma. Floating point number. Default -2.5. Sane values are -2.5 for the 2C02E PPU chip used in early NTSC NES/Famicom models, -5.0 for the 2C02G used in mid/late NTSC models, and -10.0 for the 2C07 used in PAL models (although PAL phase alternation cancels this out when `--nespalmode true`).

**CRT Simulation Parameters:**
- `--crtemu`: Specifies whether to simulate a CRT before or after gamut conversion, or not at all. Possible values are `none` (default), `front` (before the gamut conversion), and `back` (after the gamut conversion). The common use case is `front`. There are few, if any, use cases for `back`.
- `--crtmod`: Specifies the R'G'B'-to-composite modulator chip in the game console. Note that (1) on paper, these chips are all extremely close to an ideal modulator, (2) the data sheets for these chips were often more aspirational than descriptive, and (3) these chips generated all sorts of analog artifacts that aren't simulated here. Accordingly, `none` is the advised setting. Possible values are:
     - `none`  Assume an ideal modulator.(default)
     - `CXA1145` Extremely common. Used in most 1st generation and some 2nd generation Sega Genesis, Sega Master System II, NEO GEO AES, Amiga consoles, SNK consoles, and many other things. [insert cite]
     - `CXA1645` Used in some 2nd generation and all 3rd generation Sega Genesis, Sony Playstation 1, Genesis 3, Sega Saturn, NeoGeo CD/CDZ. [insert cite]
- `--crtdemod`: Specifies the composite-to-R'G'B' demodulator chip in the CRT television. CRT "color correction" is accomplished via the demodulation angles and gains built into this chip. Possible values are:
     - `none` Do not simulate demodulation. Equivalent to an ideal "plain-vanilla" demodulator. (default)
     - `dummy` Assume an ideal "plain-vanilla" demodulator. Use this for chips implementing "no color correction" standards such as EBU and SMPTE-C. Same as `none` unless `--crt-hue-knob` is non-zero.
     - `CXA1464AS` Used in Japan Sony Trinitron ~1993-1995.
     - `CXA1465AS` Used in U.S. Sony Trinitron ~1993-1995.
     - `CXA1870S_JP` Used in Japan Sony Trinitron ~1996.
     - `CXA1870S_US` Used in U.S. Sony Trinitron ~1996.
     - `CXA2060BS_JP` Likely used in Japan Sony Trinitron ~1997.
     - `CXA2060BS_US` Likely used in U.S. Sony Trinitron ~1997.
     - `CXA2060BS_PAL` Likely used in Europe Sony Trinitron ~1997.
     - `CXA2025AS_JP` Used in Japan Sony Trinitron ~1997.
     - `CXA2025AS_US` Used in U.S. Sony Trinitron ~1997.
     - `CXA1213AS` Probably used in Sony Trinitron ~1992. Unclear if this chip was meant for Japan, U.S., or both.
     - `TDA8362` Used in Hitachi CMT2187/2196/2198/2199 ~1994. Unclear if this television used different whitepoints in U.S. and Japan.
     - `rca_colortrak` Unknown chip used in RCA Colortrak Remote (US model, 1989). Behavior derived from empirical measurements [new16].
- `--crtdemodfixes`: Specificies whether to auto-correct low-precision values for demodulator angles and gains that are likely truncations of known values used for "plain-vanilla" demodulation, using full precision "plain-vanilla" values instead. Possible values are `true` (default) or `false`.
- `--crtdemodrenorm`: Specifies the conditions for renormalizing demodulator gains. Possible values are `none` (never renormalize), `insane` (only if both the B-Y angle is non-zero and the B-Y gain is non-one)(default), `nonzeroangle` (if the B-Y angle is non-zero), `gainnot1` (if the B-Y gain is non-one), or `all` (if either B-Y angle is non-zero or the B-Y gain is non-one). Presently, CXA1213AS and TDA8362 are the only implemented demodulators that meet any of these criteria. CXA1213AS seems to give better results without renormalization, while TDA8362 gives better results with it. Hence the default.
- `--crt-hue-knob` or `--chk`: Apply a global rotation, in degrees, to demodulation axes. Floating point number. Default 0.0. Note: The original angles are used for purposes of `--crtdemodfixes`, while the modified angles are used for purposes of `--crtdemodrenorm`.
- `--crt-saturation-knob` or `--csk`: Amplify or attenuate C signal prior to demodulation. Non-negative floating point number. Default 1.0.
- `--crtyuvconst`: Set the precision for the white balance constants used in demodulation equations. Possible values are `2digit` (truncated constants from 1953 standard), `3digit` (less truncated constants from 1994 SMPTE-C (170M) standard)(default), or `exact` (compute precise constants from 1953 primaries and Illuminant C).
- `--crtclamplow`: Specifies low clamping level for out-of-bounds R'G'B' output from demodulation. Floating point number -0.1 to 0. Default -0.1.
- `--crtclamphighenable`: Specifies whether to clamp high out-of-bounds R'G'B' output from demodulation. Possible values are `true` (default) or `false`.
- `--crtclamphigh`: Specifies high clamping level for out-of-bounds R'G'B' output from demodulation. Floating point number >= 1.0. Default 1.1. Sane values are 1.0 to 1.2, particularly 1.0, 1.04, and 1.1. Does nothing unless `--crtclamphighenable true`.
- `--crtblack`: Specifies black level for CRT EOTF function in 100x cd/m^2. Floating point number. Default 0.0001. Sane values are 0.0001 to 0.001 (0.01 to 0.1 cd/m^2). [new10] The default is probably close to a properly calibrated Sony Trinitron. [new1]
- `--crtwhite`: Specifies white level for CRT EOTF function in 100x cd/m^2. Floating point number. Default 1.71. Sane values for aperature grille CRTs are around 1.7 to 1.8 (170 - 180 cd/m^2). The default is probably close to a properly calibrated Sony Trinitron. [new1], [new2]. Shadow mask CRTs were substantially dimmer. Some professional-grade televisions may have aimed for 100 cd/m^s because that was the standard.

**Gamut Parameters:**
- `--source-primaries` or `-s`: Specifies the color primaries of the source gamut. Possible values are:
     - `srgb_spec` The sRGB/bt709 specification. Used by modern (SDR) computer monitors and modern HD video. [insert cite]
     - `ntsc_spec` The original 1953 NTSC color primaries. Used for the U.S. broadcast specification until 1994 (actually still in use until 2000ish) and the Japanese broadcast specification for the entire lifetime of SDR television. [insert cite]
     - `smptec_spec` The SMPTE-C (170M) specification. U.S. broadcast/phosphor specification from 1994. (Widespread adoption achieved 2000ish.) [insert cite]
     - `ebu_spec` The EBU specification. European broadcast/phosphor specification. [insert cite]
     - `rec2020_spec` Wide gamut specification for modern HDR monitors. [insert cite]
     - `P22_average` "Average" P22 phosphors used by grade.slang shader for Retroarch. Described as a "[m]ix between averaging KV-20M20, KDS VS19, Dell D93, 4-TR-B09v1_0.pdf and Phosphor Handbook 'P22.'" [new3]
     - `P22_trinitron` Official chromaticity coordinates for P22 phosphors in Trinitron CRT computer monitor provided by Sony to the authors of [new4], published in 1995. It's plausible that the same phosphors were used in Trinitron CRT televisions from the same time period. Default.
     - `P22_trinitron_bohnsack` Measurements of a GDM-17SE1 Trinitron CRT computer monitor (model launched 1994) reported in [new2]. Very close to `P22_trinitron`.
     - `P22_trinitron_raney1` Measurements of a Sony PVM 20M2U professional-grade CRT television (model launched ~1996). See [insert cite].
     - `P22_trinitron_raney2` Measurements of a Sony PVM 20L2MDU professional-grade CRT television (model launched 2002). See [insert cite].
     - `P22_trinitron_mixandmatch` Mixes values from `P22_trinitron` and `P22_trinitron_raney1`, picking those nearest the `ntsc_spec` coordinates. An idealized (and perhaps slightly fictional) Trinitron professional-grade CRT television.
     - `P22_nec_multisync_c400` Chromaticity coordinates for P22 phosphors in a NEC MULTISYNC C400 computer monitor (model launched ~1996), stated in [new5]. (The author of [new5] had a source for this, but forgot to include a long-form citation.)
     - `P22_dell` Chromaticity coordinates for P22 phosphors in Dell computer monitors ("all monitors except 21" Mitsubishi p/n 65532") according to 1999 e-mail from Dell support representative. [new5], [new6]
     - `P22_japan_specific` "Japan Specific Phosphor" used in Japanese broadcast master monitors until around 1996 (phased out in favor of EBU and SMPTE-C phosphors), as described in [insert cite ARIB TR B9 v1.0 (1998)]. 
     - `P22_kds_vs19` Chromaticity coordinates (measurements?) for P22 phosphors in a KDS VS19 computer monitor (model launched mid 90s?), stated without citation in [new5].
     - `P22_ebuish` EBUish phosphors noted in a 1992 Toshiba patent. [insert cite]
     - `P22_hitachi` Official chromaticity coordinates for P22 phosphors in CM2198 CRT computer monitor provided by Hitachi to the authors of [new4], published in 1995. Hitachi also made a CMT2198 CRT television, and it's plausible that the same phosphors were used.
     - `P22_apple_multiscan1705` Official chromaticity coordinates for P22 phosphors in Apple Multiple Scan 1705 computer monitor (manufactured 1995-1997). Whitepoint was 9300K+8mpcd. [new14], [new15]
     - `P22_rca_colortrak_patchy68k` Measurements of a RCA Colortrak Remote CRT television (US model, 1989). [new16]
     - `customcoord` Use the coordinants supplied by the user via `--source-primaries-custom-coords`.
- `--source-primaries-custom-coords` or `-spcc`: Specify the CIE 1931 chromaticity coordinates for the color primaries of the source gamut as a comma-separated list (no spaces!) in the following order: redx,redy,greenx,greeny,bluex,bluey. For example: `0.621,0.34,0.281,0.606,0.152,0.067`. Does nothing unless `--source-primaries customcoord`.
- `--source-whitepoint` or `--sw`: Specifies the whitepoint of the source gamut. Possible values are:
     - `D65` Whitepoint for modern sRGB, rec2020, and HD specifications, U.S. SD television specification post-1994, and European SD television specification. Some CRTs with a nominal D65 whitepoint had a higher temperature whitepoint in practice. Such as ~6900K or ~7000K in Europe and ~7000K-7500K in the U.S. [new5]
     - `9300K27mpcd` Whitepoint for Japanese SD television reciever specification. [new7] Some CRTs with a nominal 9300K+27mpcd whitepoint had a lower temperature whitepoint in practice, such as 9300K+8mpcd, 8800K, or 8500K. Default. [new5]
     - `9300K8mpcd` Whitepoint for Japanese SD television broadcast master monitors. [new7] Also very common in practice for Japanese professional-grade CRT televisions, some Japanese consumer televisions, and for computer monitors worldwide. Average of coordinates stated in [new7] and [new13].
     - `illuminantC` Nominal whitepoint for U.S. SD television pre-1994. Replaced by D65 in practice decades earlier.
     - `6900K` Approximate actual whitepoint of some "D65" CRTs.
     - `7000K` Approximate actual whitepoint of some "D65" CRTs.
     - `7100K` Approximate actual whitepoint of some "D65" CRTs.
     - `7250K` Approximate actual whitepoint of some "D65" CRTs.
     - `D75` Approximate actual whitepoint of some "D65" CRTs.
     - `8500K` Approximate actual whitepoint of some "9300K" CRTs.
     - `8800K` Approximate actual whitepoint of some "9300K" CRTs.
     - `trinitron_93k_bohnsack` Measured whitepoint of a GDM-17SE1 Trinitron computer monitor (model launched 1994). See [insert cite]. Very near 9300K+8mpcd.
     - `trinitron_d65_soniera` Measured whitepoint of a Sony Trinitron PVM-20L5 professional-grade CRT television (model launched 2002). See [insert cite]. 6480K, near D65.
     - `diamondpro_d65_fairchild` Measured whitepoint of a Mitsubishi Diamond Pro (unspecified model number) computer monitor with Trinitron tube in D65 mode. See [insert cite]. Rather far off D65.
     - `diamondpro_93k_fairchild` Measured whitepoint of a Mitsubishi Diamond Pro (unspecified model number) computer monitor with Trinitron tube in 9300K mode. See [insert cite]. Near 9300K+27mpcd.
     - `nec_multisync_c400_93k` Measured whitepoint of a NEC MultiSync C400 computer monitor. See [insert cite]. Near 9300K+27mpcd.
     - `kds_vs19_93k` Whitepoint for KDS VS19 computer monitor. Probably not an actual measurement, since exactly equal to 9300K+27mpcd.
     - `rca_colortrak_75k_patchy68k` Measured whitepoint of a RCA Colortrack Remote CRT television (US model, 1989). Near 7500K. [new16]
     - `customcoord` Use the coordinants supplied by the user via `--source-whitepoint-custom-coords`.
     - `customtemp` Derive coordinants from color temperature supplied by user via `--source-whitepoint-custom-temp`.
- `--source-whitepoint-custom-coords` or `--swcc`: Specify the CIE 1931 chromaticity coordinates for the whitepoint of the source gamut as a comma-separated list (no spaces!) in x,y order. For example: `0.281,0.311`. Does nothing unless `--source-whitepoint customcoord`.
- `--source-whitepoint-custom-temp` or `--swct`: Specify color temperature for the whitepoint of the source gamut, and coordinates will be estimated automatically. Floating point number. Does nothing unless `--source-whitepoint customtemp`.
- `--source-whitepoint-custom-temp-locus` or `--swctl`: Specify locus to use for estimating coordinates from color temperature. Does nothing unless `--source-whitepoint customtemp`. Possible values are:
     - `daylight` Used by the D series illuminants, e.g., D65. Uses official equation from CIE 15:2004. [insert cite] Uses post-1968 scientific constants specified by CIE. So illuminants differ slightly from their names. (E.g., "D65" is at ~6503.616K.)
     - `daylight-old` Same as `daylight`, but uses pre-1968 scientific constants. So illuminants match their names. (E.g., "D65" is at 6500K.) Default.
     - `daylight-dogway` Same as `daylight`, but uses a different approximation function that is probably more faithful to the underlying experimental data, but also probably not what CRT manufacturers were aiming for.
     - `daylight-dogway-old` Same as `daylight-old`, but uses a different approximation function that is probably more faithful to the underlying experimental data, but also probably not what CRT manufacturers were aiming for.
     - `plankian` Plankian (blackbody) radiator by which CCT is defined. Used for everything other than D-series illuminants. Uses up-to-date scientific constants (as of 2025). So CCT values differ slightly from historical practice during the CRT era. (E.g., what was "9300K" is now ~9305.02K.)
     - `plankian-old` Same as `plankian`, but uses pre-1968 scientific constants. So CCT values match their historical usage. (E.g., "9300K" means what it did back when NTSC-J whitepoints were selected.) 
     - `plankian-veryold` Same as `plankian-old`, but uses even older scientific constants from when Illuminant A was defined. Not very useful.
- `--source-whitepoint-custom-mpcd` or `--swcmpcd` Specify distance perpendicular to Plankian locus in MPCD units for source whitepoint when deriving coordinants from color temperature. Default 0.0.
- `--source-whitepoint-custom-mpcd-type` or `--swcmpcdt` Specify the type of MPCD units to use for `--source-whitepoint-custom-mpcd`. Possible values are:
     - `cie` 0.0004 delta-uv in the CIE1960 UCS. The most common meaning of "MPCD." Default.
     - `judd-macadam` 0.0005 delta-uv in MacAdam's xy-to-uv transformation equivalent to Judd's 1935 UCS. This was probably used (with small rounding errors) to define the canonical "9300K+27MPCD" whitepoint.
     - `judd` 0.0005 delta-xy in the Cartesian transform of Judd's 1935 UCS given in the appendix to that paper. Differs only slightly from `judd-macadam`. This was probably not used to define CRT whitepoints.
- `--dest-primaries` or `-d`: Specifies the color primaries of the destination gamut. Possible values are the same as for source gamut. Default is `srgb_spec`.
- `--dest-primaries-custom-coords` or `-dpcc`: Specify the CIE 1931 chromaticity coordinates for the color primaries of the destination gamut as a comma-separated list (no spaces!) in the following order: redx,redy,greenx,greeny,bluex,bluey. For example: `0.621,0.34,0.281,0.606,0.152,0.067`. Does nothing unless `--dest-primaries customcoord`.
- `--dest-whitepoint` or `--dw`: Specifies the whitepoint of the destination gamut. Possible values are the same as for `--source-whitepoint`. Default `D65`.
- `--dest-whitepoint-custom-coords` or `--dwcc`: Specify the CIE 1931 chromaticity coordinates for the whitepoint of the destination gamut as a comma-separated list (no spaces!) in x,y order. For example: `0.281,0.311`. Does nothing unless `--dest-whitepoint customcoord`.
- `--dest-whitepoint-custom-temp` or `--dwct`: Specify color temperature for the whitepoint of the destination gamut, and coordinates will be estimated automatically. Floating point number. Does nothing unless `--dest-whitepoint customtemp`.
- `--dest-whitepoint-custom-temp-locus` or `--dwctl`: Specify locus to use for estimating coordinates from color temperature. Does nothing unless `--dest-whitepoint customtemp`. Possible values are the same as for `--source-whitepoint-custom-temp-locus`.
- `--dest-whitepoint-custom-mpcd` or `--dwcmpcd` Specify distance perpendicular to Plankian locus in MPCD units for destination whitepoint when deriving coordinants from color temperature. Default 0.0.
- `--dest-whitepoint-custom-mpcd-type` or `--dwcmpcdt` Specify the type of MPCD units to use for `--dest-whitepoint-custom-mpcd`. Possible values are the same as for `--source-whitepoint-custom-mpcd-type`.

**Chromatic Adaptation Parameters:**
- `--adapt` or `-a`: Specifies the chromatic adaptation method to use when changing whitepoints. Possible values are `cat16` (default) or `bradford`.

**Spiral CARISMA Parameters:**
- `--spiral-carisma` or `--sc`: Perform selective hue rotation on certain high-saturation colors prior to gamut compression. Possible values are `true` (default) or `false`. Automatically disabled for NES palette generation.
- `--scfunction` or `--scf`: Interpolation function to use for Spiral CARISMA. Possible values are `cubichermite` (default) or `exponential`.
- `--scfloor` or `--scfl`: Specify the floor for the interpolation function used by Spiral CARISMA on a scale of 0.0 to 1.0 relative to the saturation of the cusp at any given hue. Colors less saturated than this will not be rotated at all. Floating point number. Default 0.7.
- `--scceiling` or `--sccl`: Specify the ceiling for the interpolation function used by Spiral CARISMA on a scale of 0.0 to 1.0 relative to the saturation of the cusp at any given hue. Colors more saturated than this will receive full rotation. Floating point number. Default 1.0.
- `--scexponent` or `--scxp`: Specify the exponent to use when Spiral CARISMA is configured to use an exponential function for interpolation (`--scfunction exponential`). Floating point number. 1.0 is linear. Values less than 1.0 are not recommended. Default 1.2.
- `--scmax` or `--scm`: Specify scaling factor applied to Spiral CARISMA's max rotation. Floating point number. 1.0 is full strength. 0.0 effectively disables Spiral CARISMA. Default 1.0.

**Gamut Compression Parameters:**
- `--map-mode` or `-m`: Specifies gamut mapping mode. Possible values are:
     - `clip`: No gamut mapping is performed and linear RGB output is simply clipped to 0, 1. Hue will be altered and detail in the out-of-bounds range will be lost. Not recommended.
     - `compress`: Uses a gamut (compression) mapping algorithm to remap out-of-bounds colors to a smaller zone inside the gamut boundary. Also remaps colors originally in that zone to make room. Essentially trades away some colorimetric fidelity in exchange for preserving hue and some of the out-of-bounds detail. Default.
     - `expand`: Same as `compress` but also applies the inverse of the compression function in directions where the destination gamut boundary exceeds the source gamut boundary. (Also, reverses the order of the steps in the `vp`, `vpr`, and `vprc` algorithms.) The only use for this is to prepare an image for a "roundtrip" conversion. Does not work well with CRT simulation. DEPRECATED. Use `--backwards true` instead.
- `--gamut-mapping-algorithm` or `--gma`: Specifies which gamut mapping algorithm to use. (Does nothing if `--map-mode clip`.) Possible values are:
     - `cusp`: The CUSP algorithm decribed in [old1], but with tunable compression parameters discussed below.
     - `hlpcm`: The HLPCM algorithm described in [old2], but with tunable compression parameters discussed below.
     - `vp`: The VP algorithm described in [old3], but with linear light scaling and tunable compression parameters discussed below.
     - `vpr`: VPR, a modification of the VP algorithm created for gamutthingy. The modifications are discussed below.
     - `vprc`: VPRC, a further modification of the VP algorithm created for gamutthingy. The modifications are discussed below. Default.
- `--safe-zone-type` or `-z`: Specifies how the outer zone subject to remapping and the inner "safe zone" exempt from remapping are defined. Possible values are:
     - `const-fidelity`: The standard approach in which the zones are defined relative to the distance from the "center of gravity" to the destination gamut boundary. Yields consistent colorimetric fidelity, with variable detail preservation.
     - `const-detail`: The approach described in [old4 Su] in which the remapping zone is defined relative to the difference between the distances from the "center of gravity" to the source and destination gamut boundaries. As implemented here, an overriding minimum size for the "safe zone" (relative to the destination gamut boundary) may also be enforced. Yields consistent detail preservation, with variable colorimetric fidelity (setting aside the override option).  Default.
     - Note that the behavior when `--safe-zone-type const-detail` is used in conjunction with a high minimum safe zone set by `--remap-limit` is somewhat unintuitive. This mode will never preserve more detail than `--safe-zone-type const-fidelity` with the same limit. Rather it will sometimes preserve less detail, in exchange for greater colorimetric fidelity, where the gamut boundary differences are small and presumably "enough" detail is already preserved.
- `--remap-factor` or `--rf`: Specifies the size of the remapping zone relative to the difference between the distances from the "center of gravity" to the source and destination gamut boundaries. (Does nothing if `--safe-zone-type const-fidelity`.) Floating point number 0.0 to 1.0. Default 0.4.
- `--remap-limit` or `--rl`: Specifies the size of the safe zone (exempt from remapping) relative to the distance from the "center of gravity" to the destination gamut boundary. If `--safe-zone-type const-detail`, this serves as a minimum size limit when application of `--remap-factor` would lead to a smaller safe zone. Floating point number 0.0 to 1.0.  Default 0.9.
- `--knee` or `-k`: Specifies the type of knee function used for compression, `hard` or `soft` (default).
- `--knee-factor` or `--kf`: Specifies the width of the soft knee relative to the size of the remapping zone. (Does nothing if `--knee hard`.) Note that the soft knee is centered at the knee point, so half the width extends into the safe zone, thus expanding the area that is remapped. Floating point number 0.0 to 1.0. Default 0.4.

**Output Parameters:**
- `--outfile` or `-o`: Specifies output file. For .png file conversion and LUT generation, the output will be a .png file. For NES palette generation, the output will be a .pal file usable by most NES emulators.
- `--neshtmloutputfile`: Specifies a secondary output file for writing a NES palette in human-readable html.
- `--gamma-out` or `--gout`: Specifies the inverse gamma function to be applied to the output. Possible values are `srgb` (default), `linear`, and `rec2084`. Will be ignored if CRT simulation after gamut conversion is enabled (`--crtemu back`) since the CRT inverse EOTF function will be used instead. (Note that `rec2084` is not very useful since 16-bit png output isn't supported yet.)
- `--hdr-sdr-max-nits` or `--hsmn`: Specific max nits used to display SDR white on a HDR monitor for rec2084 gamma. Floating point number. Default 200.0. Sane values are ~150 to ~200. This should be documented in your monitor's user manual. Google Chrome defaults to 200 if autodetection fails [insert cite].
- `--dither` or `--di`: Specifies whether to apply dithering to the output. Possible values are `true` (default) or `false`. Uses Martin Roberts' quasirandom dithering algorithm described in [old5]. Automatically disabled for single-color input, LUT generation, and NES palette generation.

**Misc Parameters:**
- `--help` or `-h`: Displays help.
- `--verbosity` or `-v`: Specify verbosity level. Integer numbers 0-5. Default 2.

#### Usage Tips
- Destination primaries and whitepoint should generally be sRGB spec and D65. (Unless you're trying to prepare something for roundtrip conversion.)
- There are two general approaches to color grading CRT-era games: Matching a television you personally used to own, or matching a television similar to what the graphic artist used when making the game. In the latter case, you generally need to know when the game was developed and in what country.
     - **Japan:** P22 phosphors, whitepoint near 9300K, color correction via Japan mode/model demodulator chip. (A few expensive/professional models may have used EBU spec phosphors.) Some example combinations that look plausible in practice:
          - `--source-primaries P22_trinitron --source-whitepoint 9300K27mpcd --crtemu front --crtdemod CXA1464AS`
          - `--source-primaries P22_trinitron_mixandmatch --source-whitepoint 9300K8mpcd --crtemu front --crtdemod CXA2060BS_JP`
	- **U.S.:** The SMPTE-C 170M standard was issued in 1994, but adoption was not instantaneous. Many (most?) mid-90s CRT televisions still had color correction suitable for broadcasts using the old standard. For games made by U.S. developers from 1994 to 2000ish, you may have to try both possibilities and see which looks more plausible.
	     - Old standard: P22 phosphors, whitepoint near D65, color correction via U.S. mode/model demodulator chip. Some example combinations that look plausible in practice:
	          - `--source-primaries P22_trinitron_mixandmatch --source-whitepoint 7100K --crtemu front --crtdemod CXA2060BS_US`
	          - TODO: Another good looking US example
        - New standard: P22 phosphors, whitepoint near D65, no color correction. For expensive/professional models, SPMTE-C spec phosphors, whitepoint exactly D65, no color correction.
   - **Europe/Australia:** P22 phosphors, whitepoint near D65, no color correction. For expensive/professional models, EBU spec phosphors, whitepoint exactly D65, no color correction. Plausible looking example:
        - `--source-primaries ebu_spec --source-whitepoint D65 --crtemu front --crtdemod dummy`
- Unfortunately, while we have several data points in the categories of phosphor chromaticities, whitepoint chromaticity, and color correction behavior, we do not have any cases where we can say with certainty that a particular trio of phosphors, whitepoint, and color correction were used together in a particular model of television. So you will have to guess. Yellow is most strongly impacted by color correction behavior. If you can find a combination where yellows are neither too orange nor too green, then everything else will probably look good too. Some hints for calibrating around yellow:
     - Toggle Spiral CARISMA. Generally Spiral CARISMA makes primary/secondary colors look better, but occasionally things look better without it.
     - Lower whitepoint temperature makes yellows oranger; higher whitepoint temperature makes them greener.
     - Clipping demodulator output closer to 1.0 makes yellows oranger; clipping higher or not clipping at all makes them greener. (Clipping lower also makes red darker but more saturated, and reduces red-on-red detail.)
     - Try a different demodulator. You can see the angles and gains by looking in the source code in constants.h. Larger red and green angles make yellows oranger; smaller angles make them greener. Red and green gains have proportionate effects, obviously.
     - Some combinations simply do not work. Each demodulator chip was intended to pair with a particular set of phosphors (and whitepoint). If you try use certain phosphors with a demodulator intended for very different phosphors, there may be no set of parameters that look good.
     - Some combinations may be missing pieces. Each demodulator chip was intended to pair with a particular set of phosphors (and whitepoint). But there is no guarantee that both the phosphors and demodulator are represented here.
- To compute hardcoded colors for things like FFNX hext files and mark.dat in FFNx's NTSC-J mode use `--color` to convert a single color.
- LUT generation while simulating a CRT can only produces LUTs that use R'G'B' input. The program using the LUT must perform linearization in order to compute correct weights for interpolating between the 8 nearest lookup values.
- When generating a LUT, you may wish to use sRGB output gamma so that more bandwidth is dedicated to darker colors. (Dark blue, in particular, tends to develop artifacts if starved of bandwidth.) However, if you do so, then the program using the LUT must linearize the lookup values before interpolating between them.
- Unfortunately, Retroarch does not support 256x256x256 LUTs. 128x128x128 works though.

#### Implementation Details:

**General:**

PNG plumbing shamelessly borrowed from png2png example by John Cunningham Bowler.

Dithering is done using Martin Roberts' quasirandom dithering algorithm described in [old5].


**NES/Famicom Simulation:**

Unlike most other game consoles, which generate color output in R'G'B' form and then encode that to composite, the NES/Famicom produces color output in a unique "hue, luma, emphasis" form and then encodes that to composite. The NES/Famicom simulation reproduces the construction of the composite signal from hue, luma, emphasis, then decodes the composite signal to Y'UV, and then ultimately to R'G'B' which can be processed by gamutthingy's gamut conversion and compression stages. The NES/Famicom simulation is substantially a port of palgen-persune [insert cite], with Drag's palette generator [insert cite] also consulted. There is some strange behavior due to the shape of certain traces used to perform "emphasis," and also a global phase shift as luma increases. The forums at nesdev.org are an invaluable source of information on such oddities, and a post collecting relevant links can be found in [insert cite].

A NES palette does not constitute a color "gamut" *per se* because it's sparse rather than contiguous. This necessitates some special treatment during the gamut operations. First, Spiral CARISMA is force disabled for NES palette generation. Second, every palette entry (except for the achromatic ones) is presumed to sit on the source gamut boundary for purposes of gamut compression.

**Other Game Console R'G'B'-to-Composite Modulation:**

The simulation of R'G'B'-to-composite modulation computes a R'G'B'-to-Y'UV matrix from the white voltage, burst voltage, R/G/B-to-burst ratios, and R/G/B angles stated in the data sheet for the specified modulator chip. This is not a terribly useful simulation because the data sheets usually claimed to be within a rounding error of being a perfect modulator. Actual modulator chip behavior was in fact quite imperfect and varied. But reflecting that would require circuit-level emulation, which is beyond both my electrical engineering knowhow and my library of reference information on these chips.

**CRT Simulation:**
Gamutthingy's CRT emulation is comprised of four major stages: Color correction simulation, clamping, gamma simulation, and source gamut boundary pruning.

CRT televisions sets in the U.S. and Japan typically had a built-in "color correction" feature to compensate for the large difference between the broadcast specification primaries and their actual phosphor chromaticites. This was, in essence, a crude method of gamut conversion. The dominant method of color correction used in CRT television sets was to incorporate a R'G'B'-to-R'G'B' correction matrix into the into the angle and gain constants used for demodulating the composite signal as first explained in [new8] in 1966. While this is a terrible way to do gamut conversion, the method is entirely analog and cheap because it requires no additional components. Likely for these reasons, this remained the dominant method of color correction in even throughout the 1990s.

Gamuthingy simulates this method of color correction by recovering the R'G'B'-to-R'B'G' correction matrix from the demodulation angles and gains specified in the data sheets for the "jungle chips" used in various 90s-era television sets. (The chip used in a given model can generally be found in its repair manual. This and other useful information is available in the CRT Database. [new9]) The simulation has a few tunable parameters:
- The precision of the white balance constants used for demodulation can be selected from the 2-digit truncation in the 1953 standard, the 3-digit truncation in the 1994 standard, or full precision re-derivation from the CIE 1931 coordinates of Illuminant C and the 1953 primaries.
- Angle and gain values that are likely truncations of values used in "plain-vanilla" demodulation can be optionally auto-corrected to full precision.
- Values from data sheets using unusual normalization for gains can be optionally renormalized. (Typically, B-Y has angle 0 and gain 1.0, but some data sheets do odd things.)

Application of the color correction matrix results in some values that are out-of-bounds both above 1 and below 0. Pure red is the most extreme case, usually winding up around 1.3. Whether and how much CRT televisions clamped the output from the demodulation/color correction stage is a great mystery. Despite considerable effort, I've found zero authoritative sources on this topic. Accordingly, the implementation here is based on *speculation*: On the one hand, CRT televisions certainly could and did accept inputs and produce outputs brighter than "white" and darker than "black." (So the range of 0-1 R'G'B' input is really *not* coextensive with the phosphor gamut, as we always conveniently assume.) Also, clipping the upper quarter of pure reds together seems insane. (Though less so when considering that broadcasts weren't supposed to go above 75% colors.) On the other hand, outputting a 1.3 pure red sounds pretty wild too. So it seems likely that *some* clipping took place but probably looser than 0-1. Although nothing compels it to be so, we might assume the clipping after the the demodulation/color correction stage maybe resembles the clipping used for the television broadcasts that make up the expected input. Broadcast signals absolutely must be clipped to -20 to 120 IRE in order to be modulated onto the carrier wave. Some internet comments suggest clipping broadcast signals to 104 or 110 IRE "super white" was common. Grading programs like DaVinci Resolve offer clipping modes of 0 to 100, -10 to 110, and -20 to 120 IRE. So we might *guess* that clipping here was possibly to the range -0.2 to 1.2 or narrower. As a practical matter, values below -0.1 cannot be permitted because the pow() in the Jzazbz PQ function starts throwing out complex numbers around that point. So the user is given the choice to clamp low at any value between -0.1 and 0 and to clamp high at any value or not at all, with defaults at -0.1 and 1.1. To whatever extent the output from the demodulation/color correction stage still exceeds 0-1 after clamping, that will be passed along and ultimately dealt with by the gamut compression function. This "deferred" approach to clamping works surprisingly well. The main consequences of not clamping high values at all and simply deferring are:
- High out-of-bounds colors (notably red) end up brighter and less saturated than if clamped.
- Colors in the same direction as an extreme outlier (for instance 95% red) with be compressed slightly more than they would be than if the outlier were clamped.
- The gamut compression function does a better job of avoiding detail loss through clipping. (Which means it might be "finding" detail that a real CRT clipped.)
- Clamping causes some hue distortion that might be desirable as a matter of accuracy.

Gamuthingy simulates the "gamma" behavior of CRT televisions using the BT.1886 Appendix 1 EOTF function. [new10] The function from Appendix 1 is more faithful than the fairly useless Annex 1 function, which is just 2.4 gamma. The implementation here departs from the specification in a few minor ways:
- The function has been modified to handle negative inputs similar to how xvYCC handles them. [new11]
- The specification calls for calibrating the constant "b" such that the EOTF function's output matches an empirical measurement of the CRT television when both are given an input of 16/876. This is impossible without access to actual CRT televisions, and the value 16/876 is only meaningful in the context of HDR applications anyway. Accordingly, gamutthingy instead calibrates the constant "b" such that the EOTF function's output matches the specified black level when given an input of zero.
- The function's output is specified in absolute cd/m^2. This is useless outside the context of HDR applications. Gamutthingy renormalizes the output to 0-1 according to the specified black and white levels.

While European standards specified a much higher display gamma of 2.8, according to [new12] European CRT televisions exhibited the same EOTF behavior in practice as their American (and Japanese) counterparts. Accordingly, the same BT.1886 Appendix 1 EOTF function is used for all CRT simulations.

The range of outputs from the CRT color correction stage is not coextensive with the color gamut definited by the phosphors' chromaticites. Unless aggressively clamped, there will be out-of-bounds values. Additionally, some areas of the phosphor gamut may be simply unreachable given the color correction matrix. Both discrepancies cause problems if the phosphor gamut boundaries are used for gamut compression. In the former case, out-of-bounds values get clipped, and detail is lost. In the latter case, some colors may be unecessarily compressed to make room for unreachable colors. The solution employed here is to perform the gamut boundary sampling (see below) for the source gamut with reference to whether a given sample is inside the range of possible outputs from the CRT color correction stage, rather than whether it's inside the phosphor gamut.

 
**Gamut Operations:**

When converting between gamuts with different white points, chromatic adaptation is done via the the "CAT16" method described in [old10] or the "Bradford method" described in [old6] (see also [old7]). CAT16 is the default.

Gamut mapping operations are done in the JzCzhz colorspace, the polar cousin to Jzazbz, described in [old8]. A couple notes on JzCzhz:
- Scaling the units of the XYZ input to set the absolute brightness causes the hue angles to rotate. Most of this rotation happens at very low brightness. For example, linear RGB red (1,0,0) rotates about 10 degrees going from 1 nit to 100 nits, but only about 1 degree going from 100 nits to 10,000 nits. I don't know if this is just a flaw in Jzazbz's design or an accurate depiction of some brightness-hue interaction like the Bezold–Brücke shift, or something else entirely. In part to avoid any problems here, everything is scaled to 200 nits.
- The PQ and inverse PQ functions used in Jzazbz -> XYZ conversion can sometimes produce NAN outputs. Without doing a formal analysis and proof, I *assume* this is *always* the result of asking pow() to do something that leads to an imaginary or complex number, and *only* happens on inputs that fall outside any possible gamut.

Gamut boundaries are sampled as described in [old9]. Hue is sampled in 0.2 degree slices. For the coarse sampling phase, luminance is sampled in 30 steps from black to white, and chroma is sampled in 50 steps from neutral to 110% of the highest chroma value found among the red, green, and blue points. For the fine sampling phase, luminance is sampled in 50 steps and chroma is sampled in 20 steps between coarse samples. To find the boundary in a given direction, the boundary in each of the adjacent sampled hue slices is found via 2D line-line intersection, then extrapolated for the input hue via 3D line-plane intersection. To find the luminance of the cusp at a given hue, the weighted average of the cusp luminance of the adjacent sampled hue slices is used.

Five gamut mapping algorithms are available:
- CUSP, described in [old1 Morovic].
- Hue Lightness Preserving Chroma Mapping (HLPCM), described in [old2 Addari]. (Note that this algorithm has been around under various names since at least the 1990s.)
- Vividness-Preserved (VP), described in [old3 Xu].
- Vividness-Preserved, Reversed Steps (VPR), a modification of VP created for gamutthingy. The modifications are discussed below.
- Vividness-Preserved, Reversed Steps, CUSP (VPRC), a further modification of VP created for gamutthingy. The modifications are discussed below.

Linear lightness scaling is always used. This is why CUSP is used instead of its successors GCUSP or SGCK. HLPCM starts from the assumption that lightness scaling is either not needed or already done. VP has a complex lightness scaling step that is replaced here with simple linear scaling. There are four reasons for this: First, we simply don't have one good value for the typical brightness of the devices in our core use cases. Consumer television and PC monitor manufacturers generally ignore the specs and seek to offer as much brightness as possible. Thus maximum brightness varies between manufacturers and models (particularly between aperature grille and shadow mask), and is very rarely documented. (For instance, [wikipedia's article on comparing display technologies](https://en.wikipedia.org/wiki/Comparison_of_CRT,_LCD,_plasma,_and_OLED_displays) cites only one single test of one single CRT television.) Second, to the extent we have to make a guess, the maximum brightness of late-90s CRT televisions, modern SDR monitors, and modern HDR monitors in SDR mode are (very) roughly in the same neighborhood -- around 200 nits. Third, to the extent we have to make a guess, late-90s CRT televisions were probably slightly dimmer than modern monitors, leading to the situation for our main use case (NTSC-J and sRGB) where the dimmer gamut is the wider one. This is the reverse of what's expected in all the literature, and tends to undermine the rationales for various lightness scaling regimes. Fourth, I'm uneasy about working in Jzazbz on inputs of differing max lightness. (See above.) For all these reasons, lightness is simply linearly scaled to 0 to 200 nits.

The compression function is tunable. It's possible to move the knee point, use a hard or soft knee with a tunable width, or to use the unique approach to zone definition from [old4]. Typically the outer zone subject to remapping and the inner "safe zone" exempt from remapping are defined relative to the distance from the "center of gravity" to the destination gamut boundary, but [old4] defines them relative to the difference between the distances from the "center of gravity" to the source and destination gamut boundaries. In this mode, an overriding minimum size for the "safe zone" can also be set. CUSP does not seem to benefit from [old4]'s approach, but the other GMAs appear to. A soft knee seems intuitively superior to a hard knee, but it often makes little difference in practice when quantizing to RGB8 in the end anyway.

The VP paper is unclear about whether the third step is applied generally or only to below-the-cusp colors. Both possibilites are slightly problematic: If this step is applied across the entire lightness range, some above-the-cusp colors are unnecessarily desaturated. Colors in the remapping zone are pulled back from the boundary, but there are no out-of-bounds colors to fill in the space thus vacated (because step two pulled them all below the cusp). On the other hand, if step three is applied only to below-the-cusp colors, it causes a discontinuity at the cusp's luminosity where funny stuff may happen, possibly including inversion of relative chroma between formerly adjacent colors. Since it seems the slightly more plausible interpretation, gamutthingy applies the third step below the cusp only. VPR was devised as a solution to this problem.

VPR solves the problem with VP's third step by reversing the order of the second and third steps, with some modifications to the new second step. In preparation for VPR's second step, a temporary working gamut boundary is constructed for each gamut by discarding the above-the-cusp boundary segments and replacing them with a segment starting at the cusp, going in the direction away from the black point, to somewhere up above the maximum luminosity. VPR's second step does chroma-only compression using these working gamut boundaries, similar to VP's third step. VPR's third step does compression towards the black point, identical to VP's second step.

VPRC is the same as VPR except that, during the second step, below-the-cusp colors are compressed towards a point on the neutral axis with the same luminosity as the cusp (the same as in the CUSP algorithm), rather than chroma-only compression. This results in dark colors that are slightly brighter and more saturated. The rationale for this is twofold: First, the literature is nearly unanimous that compressing dark colors towards a higher luminosity looks better than chroma-only compression. Second, this reduces artifacts in dark blues when bit depth is insufficient (for example if 64x64x64 LUT stores linear RGB values).

Spiral CARISMA performs selective hue rotation on certain high-saturation colors prior to gamut compression. Generally gamut compression aims for hue constancy. However, there are some circumstances when hue rotation is preferable: First, if the destination gamut is shallow at the hue angle of a primary or secondary color in the source gamut, the compressed primary/secondary color will be disappointingly desaturated. Second, if the destination gamut is rather acute at a primary or secondary color, this will result in an input color near the source primary/secondary color compressing to an output color that is noticeably more saturated than the output for the source primary/secondary color. (E.g., some "off-red" color ends up redder than red does.) The original CARISMA algorithm solved these problems by rotating the source gamut's primary and secondary colors towards the destination gamut's. [insert cite] However, this rotation was applied to *everything*, largely undoing the gamut conversion. Spiral CARISMA fixes this by only rotating saturated colors and only when doing so is actually an improvement. The algorithm works as follows:
- First, the maximum rotation for each primary and secondary color is found by testing possible rotations in small steps from zero to full rotation towards the destination primary/secondary color. For each test rotation, the source primary/secondary color is rotated, then compressed to the destination gamut boundary, and then the distance to the original source primary/secondary color is measured. The rotation leading to the shorst distance is selected. (In practice, the winner is virtually always either zero rotation or full rotation.)
- Second, the maximum rotation for each hue slice is computed by linear interpolation between the maximum rotations of the nearest primary and secondary colors.
- Third, the rotation to use for a given color is determined by interpolating between zero rotation and the maximum rotation for the hue slice according to the color's saturation relative to the saturation of the cusp in that hue slice. The default interpolation method uses zero rotation below 0.7 relative chroma and a cubic hermite spline from 0.7 to 1.0. Those parameters are tunable, and linear or exponential interpolation can be used instead.
- Note that Spiral CARISMA necessitates a total recalculation of the source gamut boundaries since the most saturated members of a given hue slice may have rotated away somewhere else, while new colors may have rotated in.
- If all the primary and secondary colors happen to require rotation in the same direction, the resulting rotations when viewed from above the whitepoint form a spiral. Hence the name.

The user is able to input custom coordinates for gamut primaries, as well as custom coordinates or a custom color temperature for gamut whitepoints. The function for estimating chromaticity coordinates from a color temperature is borrowed from the grade.slang Retroarch shader. [new3] Unfortunately, Grade doesn't cite where this function came from. I chose this approximation function for gamutthingy because gives the most accurate estimate for D65 among the options I could find.



TODO: fill in missing citations, then alphabetize



**References:**
- [old1] Morovic, Ján. "To Develop a Universal Gamut Mapping Algorithm." Ph.D. Thesis. University of Derby, October 1998. ([Link](https://ethos.bl.uk/OrderDetails.do?did=1&uin=uk.bl.ethos.302487))
- [old2] Addari, Gianmarco. "Colour Gamut Mapping for Ultra-HD TV." Master's Thesis. University of Surrey, August 2016. ([Link](https://www.insync.tv/wp-content/uploads/2019/12/Report_on_colour_gamut_mapping.pdf))
- [old3] Xu, Liaho, Zhao, Baiyue, & Luo, Ming Ronnier "Colour gamut mapping between small and large colour gamuts: Part I. gamut compression." *Optics Express*, Vol. 26, No. 9, pp. 11481-11495. April 2018. ([Link](https://opg.optica.org/oe/fulltext.cfm?uri=oe-26-9-11481&id=385750))
- [old4] Su, Chang, Tao, Li, & Kim, Yeong Taeg. "Color-gamut mapping in the non-uniform CIE-1931 space with perceptual hue fidelity constraints for SMPTE ST.2094-40 standard." *APSIPA Transactions on Signal and Information Processing*, Vol. 9, E. 12. March 2020. ([Link](https://www.cambridge.org/core/journals/apsipa-transactions-on-signal-and-information-processing/article/colorgamut-mapping-in-the-nonuniform-cie1931-space-with-perceptual-hue-fidelity-constraints-for-smpte-st209440-standard/D2E1B9B7E0D5FCA1A5722D24F5F435A3))
- [old5] Roberts, Martin. "The Unreasonable Effectiveness of Quasirandom Sequences." *Extreme Learning*. April 2018. ([Link](https://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/))
- [old6] Lam, K.M. "Metamerism and Colour Constancy." Ph.D. Thesis. University of Bradford, 1985.
- [old7] Lindbloom, Bruce. "Chromatic Adaptation." BruceLindbloom.com. April 2017. ([Link](http://www.brucelindbloom.com/index.html?Eqn_ChromAdapt.html))
- [old8] Safdar, Muhammad, Cui, Guihua, Kim, You Jin, & Luo, Ming Ronnier. "Perceptually uniform color space for image signals including high dynamic range and wide gamut." *Optics Express*, Vol. 25, No. 13, pp. 15131-15151. June 2017. ([Link](https://opg.optica.org/fulltext.cfm?rwjcode=oe&uri=oe-25-13-15131&id=368272))
- [old9] Lihao, Xu, Chunzhi, Xu, & Luo, Ming Ronnier. "Accurate gamut boundary descriptor for displays." *Optics Express*, Vol. 30, No. 2, pp. 1615-1626. January 2022. ([Link](https://opg.optica.org/fulltext.cfm?rwjcode=oe&uri=oe-30-2-1615&id=466694))
- [old10] Li, Changjun, Li, Zhiqiang, Wang, Zhifeng, Xu, Yang, Luo, Ming Ronnier, Cui, Guihua, Melgosa, Manuel, & Pointer, Michael. "A Revision of CIECAM02 and its CAT and UCS." *Proc. IS&T 24th Color and Imaging Conf.*, pp. 208-212 ([Link](https://library.imaging.org/admin/apis/public/api/ist/website/downloadArticle/cic/24/1/art00035))
- [new1] Soneira, Raymond. "Display Technology Shoot-Out: Comparing CRT, LCD, Plasma and DLP Displays." 2005(?) ([Link](https://www.displaymate.com/ShootOut_Part_1.htm))
- [new2] Bohnsack, David L., Diller, Lisa C., Yeh, Tsaiyao, Jenness, James W., and Troy, John B. "Characteristics of the Sony Multiscan 17se Trinitron color graphic display." *Spatial Vision*, Vol. 10, No. 4, pp. 345-51. 1997. ([Link](https://www.scholars.northwestern.edu/en/publications/characteristics-of-the-sony-multiscan-17se-trinitron-color-graphi))
- [new 3] Linares, Jose (a.k.a. Dogway). "Grade - CRT emulation and color manipulation shader." Retroarch shader. 2023. ([Link](https://github.com/libretro/slang-shaders/blob/master/misc/shaders/grade.slang))
- [new4] Has, Michael and Newman, Todd. "Color Management: Current Practice and The Adoption of a New Standard."  *TAGA (Technical Association of Graphic Arts) Proceedings*, Vol. 2, pp. 748-771. 1995. ([Link](https://www.color.org/wpaper1.xalter))
- [new5] Glynn II, Earl F. "efg's Chromaticity Lab Report." *efg's Computer Lab*. July 2009. ([Link](https://web.archive.org/web/20190613001950/http://efg2.com/Lab/Graphics/Colors/Chromaticity.htm))
- [new6] Espy, Rance. E-mail to Earl F. Glynn II, "Re: Case #: 19990110193351 - Monitor." January, 1999. ([Link](https://web.archive.org/web/20171114044833/http://www.efg2.com/Lab/Graphics/Colors/DellInfo.txt))
- [new7] Yagishita, Shigeru, Nishino, Kenji, Ohta, Katsuhiro, and Ishii, Takashi. "カラーマスターモニター用基準白色内蔵カラーブラウン管 (Color Picture Tube with Built in Reference White for Color Master Monitors)." *テレビジョン* (*Television*), Vol. 31, No. 11, pp. 883-888. 1977. ([Link](https://www.jstage.jst.go.jp/article/itej1954/31/11/31_11_883/_article/-char/ja/))
- [new8] Parker, Norman W. "An Analysis of the Necessary Decoder Corrections for Color Receiver Operation with Non-Standard Receiver Primaries." *IEEE Transactions on Consumer Electronics*, Vol. CE-28, No. 1, pp. 74-83. February 1982. ([Link](https://ieeexplore.ieee.org/document/4179914))
- [new9] CRT Database ([link](https://crtdatabase.com/))
- [new10] International Telecommunication Union. "Recommendation ITU-R BT.1886: Reference electro-optical transfer function for flat panel displays used in HDTV studio production." March, 2011. ([Link](https://www.itu.int/rec/R-REC-BT.1886-0-201103-I/en))
- [new11]  Matsumoto, Tatsuhiko, Shimpuku, Yoshihide, Nakatsue, Takehiro, Haga, Shuichi, Eto, Hiroaki, Akiyama, Yoshiyuki, and Katoh, Naoya. "xvYCC: A New Standard for Video Systems using Extended-Gamut YCC Color Space." *Society for Information Display Symposium Digest of Technical Papers 2006*. 19.2, pp. 1130–1133. 2006. ([Link](https://sid.onlinelibrary.wiley.com/doi/abs/10.1889/1.2433175)) 
- [new12] Poyton, Charles. "Colour Appearance Issues in Digital Video, HD/UHD, and D‑cinema." Ph.D. Thesis. Simon Frasier University. Summer 2018. p. 125 ([Link](https://poynton.ca/PDFs/Poynton-2018-PhD.pdf))
- [new13] Nagaoka, Yoshitomi. "テレビジョンの色再現と基準白色 (On the Color Reproduction and Reference White of Color Television)." *テレビジョン学会誌* (*Journal of the Television Society*), Vol. 33, No. 12, pp. 1013-1020. 1979. ([Link](https://www.jstage.jst.go.jp/article/itej1978/33/12/33_12_1013/_article/-char/ja/))
- [new14] Apple Computer, Inc. "Multiple Scan 1705 Display: CIE Phosphor Settings." *AppleCare Tech Info Library*, Article ID 24445. March 30, 1998. ([Link](https://til-2001.mirror.kb1max.com/techinfo.nsf/artnum/n24445/index.html))
- [new15] Apple Wiki. "Apple Multiple Scan Display." ([Link](https://apple.fandom.com/wiki/Apple_Multiple_Scan_Display))
- [new16] insert cite to patchy68k


**Building:**

Linux:
- Install libpng-dev >= 1.6.0
- `make`

Windows:
- Either install libpng >= 1.6.0 where your linker knows to find it, or edit the #includes to use a local copy.
- TODO make a visual studio project file


TODO: cite NES stuff, cite CRT color correction, cite poyton PAL gamma, CRT gamma section, add res tof modulator chips, explain spiracl charisma, explain vprc, cite gamut sources

cite this
The Effect of Ultrafine Pigment Color Filters on Cathode Ray Tube Brightness, Contrast, and Color Purity

Katsutoshi Ohno1 and Tsuneo Kusunoki1

© 1996 ECS - The Electrochemical Society
Journal of The Electrochemical Society, Volume 143, Number 3 Citation Katsutoshi Ohno and Tsuneo Kusunoki 1996 J. Electrochem. Soc. 143 1063
https://iopscience.iop.org/article/10.1149/1.1836583


full cite for color .originally
Color Management: Current Practice and The Adoption of a New Standard.
カラー管理 現行の実情と新標準の適用

    Publisher site Copy service
    Access JDreamⅢ for advanced search and analysis.

Clips
Author (2)： HAS M
(International Color Consortium)
,  NEWMAN T
(International Color Consortium)

Material： TAGA Proceedings (Technical Association of Graphic Arts)  (TAGA Proc (Tech Assoc Graphic Arts))

Volume： 1995  Issue： Vol 2  Page： 748-771  Publication year： 1995
JST Material Number： B0702A  CODEN： TAPRA  Document type： Proceedings
Article type： 原著論文  Country of issue： United States (USA)  Language： ENGLISH (EN)
https://jglobal.jst.go.jp/en/detail?JGLOBAL_ID=200902173358671326
Michael Has and Todd Newman

https://www.displaymate.com/ShootOut_Part_1.htm
Sony PVM-20L5 black 0.01 cd/m2 to white 176 cd/m2

cite grade
https://github.com/libretro/slang-shaders/blob/master/misc/shaders/grade.slang

https://github.com/Gumball2415/palgen-persune/blob/main/palgen_persune.py

http://drag.wootest.net/misc/palgen.html

https://forums.nesdev.org/viewtopic.php?p=296732#p296732
