#include "nes.h"
#include "matrix.h"

#include <cstring> // for memset & memcpy
#include <numbers> // for PI
#include <math.h> // for sin/cos
#include <stdio.h> // for printf

/*
 * Simulates the PPU of a NES/Famicom for purposes of palette generation.
 * Based very heavily on:
 * https://github.com/Gumball2415/palgen-persune/blob/main/palgen_persune.py
 * and
 * view-source:http://drag.wootest.net/misc/palgen.html
 * For info on the default values, see
 * https://forums.nesdev.org/viewtopic.php?p=296732#p296732
 */

// signal LUTs
// voltage highs and lows
// from https://forums.nesdev.org/viewtopic.php?p=159266#p159266
// signal[4][2][2] luma $0x-$3x, high/low $x0/$xD, no emphasis/emphasis
const double signal_table_composite[4][2][2] = {
    {
        { 0.616, 0.500 },
        { 0.228, 0.192 }
    },
    {
        { 0.840, 0.676 },
        { 0.312, 0.256 }
    },
    {
        { 1.100, 0.896 },
        { 0.552, 0.448 }
    },
    {
        { 1.100, 0.896 },
        { 0.880, 0.712 }
    }
};
const double sync_volts = 0.048;
const double cburst_low_volts = 0.148;
const double cburst_high_volts = 0.524;
// NES uses $1D for blanking
// https://forums.nesdev.org/viewtopic.php?p=131995&hilit=porch#p131995
const double blanking_volts = signal_table_composite[1][1][0]; //0.312v
const double max_volts = signal_table_composite[3][0][0]; // 1.1v
const double nominal_IRE_divisor = 140.0; // ~0.00714v per IRE
const double sync_IRE_divisor = 40.0 / (blanking_volts - sync_volts); // 151.5151...; 0.0066v per IRE
const double burst_IRE_divisor = 40.0 / (cburst_high_volts - cburst_low_volts); // ~106.38; 0.0094v per IRE

// verboselevel: verbostiy level
// ispal: simulate PAL's alternating phases?
// skew26A: Phase skew for hues 0x2, 0x6, and 0xA due to trace design. Degrees. Sane value is ~4.5
// boost48C: Luma boost for hues 0x4, 0x8 and 0xC due to trace design. IRE. Sane value is 1.0
// skewstep: Phase skew per luma level. Degrees. Sane value depends on which chip we're simulating:
//      2C02E: ~-2.5 degrees per luma step
//      2C02G: ~-5 degrees per luma step
//      2C07: ~10 dgrees per luma step (but PAL so it cancels out)
// agcluma: What kind of automatic gain control to use for luma
// agcchroma: What kind of automatic gain control to use for chroma
bool nesppusimulation::Initialize(int verboselevel, bool ispal, double skew26A, double boost48C, double skewstep, int yuvconstprec,  int agcluma, int agcchroma, double superwhitefactor, crtdescriptor* thecrt){

    verbosity = verboselevel;
    palmode = ispal;
    phaseskew26A = skew26A;
    lumaboost48C = boost48C;
    phaseskewperlumastep = skewstep;
    YUVconstantprecision = yuvconstprec;
    agclumatype = agcluma;
    agcchromatype = agcchroma;
    superwhiteshowfactor = superwhitefactor;

    //see https://forums.nesdev.org/viewtopic.php?p=86782#p86782
    switch (agclumatype){
        case NES_AGC_LUMA_NONE:
            IRE_divisor = nominal_IRE_divisor; //results in 110.32 IRE white
            break;
        case NES_AGC_LUMA_SYNC:
            IRE_divisor = sync_IRE_divisor; // results in 119.3939... IRE white
            break;
        case NES_AGC_LUMA_BURST:
            IRE_divisor = burst_IRE_divisor; // results in ~83.83 IRE white
            break;
        default:
            break;
    }
    printf("IRE divisor is %f\n", IRE_divisor);

    switch (agcchromatype){
        case NES_AGC_CHROMA_SAME:
            chroma_IRE_correction = 1.0;
            break;
        case NES_AGC_CHROMA_BURST:
            if (agclumatype == NES_AGC_LUMA_BURST){
                chroma_IRE_correction = 1.0;
            }
            else {
                chroma_IRE_correction = burst_IRE_divisor / IRE_divisor;
            }
            break;
        default:
            break;
    }
    //printf("chroma_IRE_correction is %f\n", chroma_IRE_correction);

    // assume documentation on 48C luma boost is stated in nominal IRE
    if (agclumatype != NES_AGC_LUMA_NONE){
        lumaboost48C *= 140.0 / IRE_divisor;
    }
    //printf("lumaboost48C is %f\n", lumaboost48C);

    double whiteIRE = IRE_divisor * (max_volts - blanking_volts);
    whiteIRE /= 100.0;
    whiteIRE = pow(whiteIRE, thecrt->globalgammaadjust);
    double linearwhite = thecrt->tolinear1886appx1(whiteIRE);
    if (linearwhite > 1.0){
        double overage = linearwhite - 1.0;
        overage *= superwhiteshowfactor;
        double newlinearwhite = 1.0 + overage;
        thecrt->SetNESScaleFactor(1.0 / newlinearwhite);
        Ycliplevel = thecrt->togamma1886appx1(newlinearwhite);
        Ycliplevel = pow(Ycliplevel, 1.0 / thecrt->globalgammaadjust);
        //printf("linearwhite is %f, newlinearwhite is %f, scale factor is %f, set as %f\n", linearwhite, newlinearwhite, 1.0 / newlinearwhite, thecrt->NESrenormaliztionfactor);
    }
    else {
        thecrt->SetNESScaleFactor(1.0 / linearwhite);
        //printf("linearwhite is %f,  scale factor is %f, set as %f\n", linearwhite, 1.0 / linearwhite, thecrt->NESrenormaliztionfactor);
    }

    bool output = InitializeYUVtoRGBMatrix();

    return output;
}

// Initialize an idealized YUV to R'G'B' matrix.
// We need R'G'B' output from the NES simulation b/c the color correction built into the TV's demodulation
// is represented as a R'G'B' to R'G'B' matrix in crtemulation.cpp.
bool nesppusimulation::InitializeYUVtoRGBMatrix(){

    bool output = true;

    output = MakeIdealYUVtoRGB(idealizedYUVtoRGBMatrix, YUVconstantprecision);

    if (verbosity >= VERBOSITY_SLIGHT){
        printf("\nIdealized YUV to RGB matrix is:\n");
        print3x3matrix(idealizedYUVtoRGBMatrix);
        printf("\n----------\n\n");
    }

    return output;

}

// Compute the reversed phase for the next line in PAL mode
int nesppusimulation::pal_phase(int hue){
    if ((hue >= 1) && (hue <= 12)){
        return (-1 * (hue - 5)) % 12;
    }
    return hue;
}

// Is this hue active for this phase?
bool nesppusimulation::in_color_phase(int hue, int phase, bool pal){
    if (pal){
        return ((pal_phase(hue) + phase) % 12) < 6;
    }
    return ((hue + phase) % 12) < 6;
}

// Compute composite signal amplitude for a given NES hue/luma/emphasis triad and phase.
// If backwards == true, behave as reversed-phase PAL line.
double nesppusimulation::encode_composite(int emphasis, int luma, int hue, int wave_phase, bool backwards){

    int luma_index = luma;
    if (hue >= 0xE){
        luma_index = 0x1;
    }

    if (luma_index < 0){
        luma_index = 0;
    }
    else if (luma_index > 3){
        luma_index = 3;
    }

    bool wavemode = (palmode && backwards);

    // 0 = waveform high; 1 = waveform low
    int n_wave_level;
    if (hue == 0x0){
        n_wave_level = 0;
    }
    else if (hue >= 0xD){
        n_wave_level = 1;
    }
    else {
       n_wave_level = in_color_phase(hue, wave_phase, wavemode) ? 0 : 1;
    }

    // 1 = emphasis activate
    bool empon = ((hue < 0xE) && (
        ((emphasis & 1) && in_color_phase(0xC, wave_phase, wavemode)) ||
        ((emphasis & 2) && in_color_phase(0x4, wave_phase, wavemode)) ||
        ((emphasis & 4) && in_color_phase(0x8, wave_phase, wavemode))));
    int emphasis_level = empon ? 1 : 0;

    return signal_table_composite[luma_index][n_wave_level][emphasis_level];

}

// Convert NES hue/luma/emphasis triad to YUV
vec3 nesppusimulation::NEStoYUV(int hue, int luma, int emphasis){
    // Don't really need all these arrays; they arose from porting from python.
    // Keeping them for clarity's sake since the memory cos tis negligible.
    double voltage_buffer[12];
    double voltage_buffer_b[12];
    double voltage_bandpass[12];
    double voltage_bandpass_b[12];
    double U_buffer[12];
    double V_buffer[12];
    double U_decode[12];
    double V_decode[12];
    double U_comb[12];
    double V_comb[12];

    memset(&voltage_buffer, 0, 12 * sizeof(double));
    memset(&voltage_buffer_b, 0, 12 * sizeof(double));
    memset(&voltage_bandpass, 0, 12 * sizeof(double));
    memset(&voltage_bandpass_b, 0, 12 * sizeof(double));
    memset(&U_buffer, 0, 12 * sizeof(double));
    memset(&V_buffer, 0, 12 * sizeof(double));
    memset(&U_decode, 0, 12 * sizeof(double));
    memset(&V_decode, 0, 12 * sizeof(double));
    memset(&U_comb, 0, 12 * sizeof(double));
    memset(&V_comb, 0, 12 * sizeof(double));

    // put the composite signal into voltage_buffer
    for (int wave_phase=0; wave_phase<12; wave_phase++){
        voltage_buffer[wave_phase] = encode_composite(emphasis, luma, hue, wave_phase, false);
        // in PAL mode, we also want the next line with reversed phase into voltage_buffer_b
        if (palmode){
            int next_phase = (wave_phase + 2) % 12;
            voltage_buffer_b[next_phase] = encode_composite(emphasis, luma, hue, next_phase, true);
        }
    }

    // Due to the trace designs to accomdate emphasis bits,
    // hues 0x2, 0x6, and 0xA get a phase shift,
    // while hues 0x4, 0x8, and 0xC get a luma boost.
    // Phase shift is ~+4.5 degrees
    // Luma boost is about ~1 IRE.
    // See https://forums.nesdev.org/viewtopic.php?p=296732#p296732
    double phaseskew_pt1 = 0;
    if ((hue == 0x2) || (hue == 0x6) || (hue == 0xA)){
        phaseskew_pt1 = phaseskew26A * (std::numbers::pi_v<long double>/ 180.0); // convert to radians
    }

    double lumaboost = 0;
    if ((hue == 0x4) || (hue == 0x8) || (hue == 0xC)){
        lumaboost = lumaboost48C;
    }

    // There's also a phase shift as luma increases.
    // How much depends on which chip
    // 2C02E: ~-2.5 degrees per luma step
    // 2C02G: ~-5 degrees per luma step
    // 2C07: ~10 dgrees per luma step (but PAL so it cancels out)
    // See https://forums.nesdev.org/viewtopic.php?p=296732#p296732
    double phaseskew_pt2 = luma * phaseskewperlumastep * (std::numbers::pi_v<long double>/ 180.0); // convert to radians

    // shift black to 0 and convert from volts to IRE
    for (int i=0; i<12; i++){
        voltage_buffer[i] = IRE_divisor * (voltage_buffer[i] - blanking_volts);
        voltage_buffer_b[i] = IRE_divisor * (voltage_buffer_b[i] - blanking_volts);
    }

    // bandpass filter
    double average_voltage_buffer = 0.0;
    double average_voltage_buffer_b = 0.0;
    for (int i=0; i<12; i++){
        average_voltage_buffer += voltage_buffer[i];
        average_voltage_buffer_b += voltage_buffer_b[i];
    }
    average_voltage_buffer /= 12.0;
    average_voltage_buffer_b /= 12.0;
    memcpy(&voltage_bandpass, &voltage_buffer, 12 * sizeof(double));
    memcpy(&voltage_bandpass_b, &voltage_buffer_b, 12 * sizeof(double));
    for (int i=0; i<12; i++){
        voltage_bandpass[i] -= average_voltage_buffer;
        voltage_bandpass_b[i] -= average_voltage_buffer_b;
    }

    // comb filter
    if (palmode){
        // use two lines
        for (int i=0; i<12; i++){
            U_comb[i] = (voltage_bandpass[i] + voltage_bandpass_b[i]) / 2.0;
            V_comb[i] = (voltage_bandpass[i] - voltage_bandpass_b[i]) / 2.0;
        }
    }
    else {
        // use only one line
        memcpy(&U_comb, &voltage_bandpass, 12 * sizeof(double));
        memcpy(&V_comb, &voltage_bandpass, 12 * sizeof(double));
    }

    // decode UV
    for (int i=0; i<12; i++){

        // Not really clear on what palgen_persune.py is doing here with the backwards stuff, but porting it faithfully
        // As per palgen_persune.py:
        // subcarrier generation is 180 degrees offset
        // due to the way the waveform is encoded, the hue is off by an additional 1/2 of a sample

        // colorburst is at hue 0x8 for NTSC models, 7.5 for PAL
        double colorburst_phase = palmode ? 7.5 : 8.0;

        // The phase shift for hues 0x2, 0x6, and 0xA moves blue towards magenta, red towards yellow, and green towards cyan.
        // The phase shift per luma step moves blue towards cyan, red towards magenta, and green towards yellow (and also moves everything else in that same direction).

        // 2x due to integral of sin(2*PI*x)^2
        double saturation_correction = 2.0 * chroma_IRE_correction;

        U_decode[i] = saturation_correction * sin((((2.0 * std::numbers::pi_v<long double>) / 12.0) * (i - 1.0 - colorburst_phase - 0.5)) - phaseskew_pt1 + phaseskew_pt2);

        V_decode[i] = saturation_correction * cos((((2.0 * std::numbers::pi_v<long double>) / 12.0) * (i - 1.0 - colorburst_phase - 0.5)) - phaseskew_pt1 + phaseskew_pt2);

        U_buffer[i] = U_comb[i] * U_decode[i];
        V_buffer[i] = V_comb[i] * V_decode[i];

    }

    // lowpass filter UV
    double Uout = 0.0;
    double Vout = 0.0;
    for (int i=0; i<12; i++){
        Uout += U_buffer[i];
        Vout += V_buffer[i];
    }
    Uout /= 12.0;
    Vout /= 12.0;

    // decode Y
    double Yout = average_voltage_buffer;
    if (palmode){
        Yout = 0.0;
        for (int i=0; i<12; i++){
            Yout += voltage_buffer[i] - voltage_bandpass[i];
        }
        Yout /= 12.0;
    }
    Yout += lumaboost;

    // normalize IRE to 0-1 range
    Yout /= 100.0;
    Uout /= 100.0;
    Vout /= 100.0;

    if (Yout > Ycliplevel){
        Yout = Ycliplevel;
        // For now, just clamp Y.
        // And let gamut compression algorithm deal with out-of-bounds chroma.
        /*
        if ((Uout < -0.5) || (Uout > 0.5)){
            printf("out of bounds U for luma %i, hue %i, emp %i: U= %f\n", luma, hue, emphasis, Uout);
        }
        if ((Vout < -0.5) || (Vout > 0.5)){
            printf("out of bounds V for luma %i, hue %i, emp %i: U= %f\n", luma, hue, emphasis, Vout);
        }
        */
    }

    return vec3(Yout, Uout, Vout);

}

// Convert NES hue/luma/emphasis triad to R'G'B'
vec3 nesppusimulation::NEStoRGB(int hue, int luma, int emphasis){

    vec3 yuv = NEStoYUV(hue, luma, emphasis);

    vec3 rgb = multMatrixByColor(idealizedYUVtoRGBMatrix, yuv);

    // screen barf
    //printf("NES palette: Luma %i, hue %i, emp %i yeilds RGB: ", luma, hue, emphasis);
    //rgb.printout();

    return rgb;
}
