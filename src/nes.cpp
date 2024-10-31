#include "nes.h"
#include "matrix.h"

#include <cstring> // for memset & memcpy
#include <numbers> // for PI
#include <math.h> // for sin/cos
#include <stdio.h> // for printf

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
const double composite_black = signal_table_composite[1][1][0];
const double composite_white = signal_table_composite[3][0][0];
// set "white" as the highest signal we can get, in IRE
const double signal_white_point = 140.0 * (composite_white - composite_black);

// Colorburst is supposed to be 40 IRE, but NES's colorburst is 52.64 IRE
// In theory, TV should be normalizing chroma against the colorburst amplitude
// In practice, some did and some didn't.
const double colorburst_amp_correction = (40.0 / (140.0 * (0.524 - 0.148)));


// verboselevel: verbostiy level
// ispal: simulate PAL's alternating phases?
// cbcorrection: Normalize chroma to NES's non-standard colorburst amplitude?
// skew26A: Phase skew for hues 0x2, 0x6, and 0xA due to trace design. Degrees. Sane value is ~4.5
// boost48C: Luma boost for hues 0x4, 0x8 and 0xC due to trace design. IRE. Sane value is 1.0
// skewstep: Phase skew per luma level. Degrees. Sane value depends on which chip we're simulating:
//      2C02E: ~-2.5 degrees per luma step
//      2C02G: ~-5 degrees per luma step
//      2C07: ~10 dgrees per luma step (but PAL so it cancels out)
bool nesppusimulation::Initialize(int verboselevel, bool ispal, bool cbcorrection, double skew26A, double boost48C, double skewstep){

    verbosity = verboselevel;
    palmode = ispal;
    docolorburstampcorrection = cbcorrection;
    phaseskew26A = skew26A;
    lumaboost48C = boost48C;
    phaseskewperlumastep = skewstep;

    bool output = InitializeYUVtoRGBMatrix();

    return output;
}

bool nesppusimulation::InitializeYUVtoRGBMatrix(){


    // We need the NTSC1953 white balance factors
    // So the first part of this is copy/pasta from crtdescriptor::InitializeNTSC1953WhiteBalanceFactors()
    if (verbosity >= VERBOSITY_SLIGHT){
        printf("\n----------\nInitializing idealized YUV to RGB matrix for NES palette generation...\n");
    }

    bool output = true;

    const double matrixP[3][3] = {
        {gamutpoints[GAMUT_NTSC_1953][1][0], gamutpoints[GAMUT_NTSC_1953][2][0], gamutpoints[GAMUT_NTSC_1953][3][0]},
        {gamutpoints[GAMUT_NTSC_1953][1][1], gamutpoints[GAMUT_NTSC_1953][2][1], gamutpoints[GAMUT_NTSC_1953][3][1]},
        {gamutpoints[GAMUT_NTSC_1953][1][2], gamutpoints[GAMUT_NTSC_1953][2][2], gamutpoints[GAMUT_NTSC_1953][3][2]}
    };

    double inverseMatrixP[3][3];
    output = Invert3x3Matrix(matrixP, inverseMatrixP);
    if (!output){
        printf("Disaster! Matrix P is not invertible! Bad stuff will happen!\n");
    }

    vec3 matrixW;
    matrixW.x = gamutpoints[GAMUT_NTSC_1953][0][0] / gamutpoints[GAMUT_NTSC_1953][0][1];
    matrixW.y = 1.0;
    matrixW.z = gamutpoints[GAMUT_NTSC_1953][0][2] / gamutpoints[GAMUT_NTSC_1953][0][1];

    vec3 normalizationFactors = multMatrixByColor(inverseMatrixP, matrixW);

    const double matrixC[3][3] = {
        {normalizationFactors.x, 0.0, 0.0},
        {0.0, normalizationFactors.y, 0.0},
        {0.0, 0.0, normalizationFactors.z}
    };

    double matrixNPM[3][3];
    mult3x3Matrices(matrixP, matrixC, matrixNPM);

    const double wr = matrixNPM[1][0];
    const double wg = matrixNPM[1][1];
    const double wb = matrixNPM[1][2];

    const double matrixRGBtoYPbPr[3][3] = {
        {wr, wg, wb},
        {-1.0 * wr, -1.0 * wg, wr + wg},
        {wg + wb, -1.0 * wg, -1.0 * wb}
    };

    const double matrixYPbPrtoYUV[3][3] = {
        {1.0, 0.0, 0.0},
        {0.0, Udownscale, 0.0},
        {0.0, 0.0, Vdownscale}
    };

    double matrixRGBtoYUV[3][3];
    mult3x3Matrices(matrixYPbPrtoYUV, matrixRGBtoYPbPr, matrixRGBtoYUV);

    double matrixYUVtoRGB[3][3];
    output = Invert3x3Matrix(matrixRGBtoYUV, matrixYUVtoRGB);
    if (!output){
        printf("Disaster! MatrixRGBtoYUV is not invertible! Bad stuff will happen!\n");
    }

    memcpy(&idealizedYUVtoRGBMatrix, matrixYUVtoRGB, 9 * sizeof(double));

    if (verbosity >= VERBOSITY_SLIGHT){
        printf("\n----------\nIdealized YUV to RGB matrix is:\n");
        print3x3matrix(idealizedYUVtoRGBMatrix);
    }


    return output;

}

int nesppusimulation::pal_phase(int hue){
    if ((hue >= 1) && (hue <= 12)){
        return (-1 * (hue - 5)) % 12;
    }
    return hue;
}

bool nesppusimulation::in_color_phase(int hue, int phase, bool pal){
    if (pal){
        return ((pal_phase(hue) + phase) % 12) < 6;
    }
    return ((hue + phase) % 12) < 6;
}

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

vec3 nesppusimulation::NEStoYUV(int hue, int luma, int emphasis){
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

    for (int wave_phase=0; wave_phase<12; wave_phase++){
        voltage_buffer[wave_phase] = encode_composite(emphasis, luma, hue, wave_phase, false);
        // in PAL mode, we also want the next line with reversed phase
        if (palmode){
            int next_phase = (wave_phase + 4) % 12;
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
        voltage_buffer[i] = 140.0 * (voltage_buffer[i] - composite_black);
        voltage_buffer_b[i] = 140.0 * (voltage_buffer_b[i] - composite_black);
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
        // subcarrier generation is 180 degrees offset
        // due to the way the waveform is encoded, the hue is off by an additional 1/2 of a sample


        // colorburst is at hue 0x8.

        // TODO: TEST for a sign flip error
        // The phase shift for hues 0x2, 0x6, and 0xA moves blue towards magenta, red towards yellow, and green towards cyan.
        // The phase shift per luma step moves blue towards cyan, red towards magenta, and green towards yellow (and also moves everything else in that same direction).

        // 2x due to integral of sin(2*PI*x)^2
        double saturation_correction = docolorburstampcorrection ? 2.0 * colorburst_amp_correction : 2.0;

        U_decode[i] = saturation_correction * sin((((2.0 * std::numbers::pi_v<long double>) / 12.0) * (i - 1.0 - 8.0 - 0.5)) - phaseskew_pt1 + phaseskew_pt2);

        V_decode[i] = saturation_correction * cos((((2.0 * std::numbers::pi_v<long double>) / 12.0) * (i - 1.0 - 8.0 - 0.5)) - phaseskew_pt1 + phaseskew_pt2);

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
    // (Though we still may have out-of-bounds values b/c NES does crazy things.)
    Yout /= signal_white_point;
    Uout /= signal_white_point;
    Vout /= signal_white_point;

    return vec3(Yout, Uout, Vout);

}

vec3 nesppusimulation::NEStoRGB(int hue, int luma, int emphasis){

    vec3 yuv = NEStoYUV(hue, luma, emphasis);

    vec3 rgb = multMatrixByColor(idealizedYUVtoRGBMatrix, yuv);

    // screen barf
    //printf("NES palette: Luma %i, hue %i, emp %i yeilds RGB: ", luma, hue, emphasis);
    //rgb.printout();

    return rgb;
}
