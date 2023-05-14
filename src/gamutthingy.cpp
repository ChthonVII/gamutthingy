// g++ -std=c++20 -o gamutboundaries gamutboundaries.cpp -lm

//g++ -std=c++20 -o gamutboundaries gamutboundaries.cpp -lpng16 -lz -lm

//https://stackoverflow.com/questions/47649507/3d-line-segment-and-plane-intersection-contd

//#include <stddef.h>
//#include <stdlib.h>
#include <string.h>
#include <stdio.h>
//#include <stdbool.h>
//#include <math.h>
//#include <limits>
//#include <cfloat>
//#include <vector>
#include <string>
//#include <numbers>
#include <errno.h>

/* Normally use <png.h> here to get the installed libpng, but this is done to
 * ensure the code picks up the local libpng implementation:
 */
//#include "../../png.h"
#include <png.h> // Linux should have libpng-dev installed; Windows users can figure stuff out.
#if defined(PNG_SIMPLIFIED_READ_SUPPORTED) && \
    defined(PNG_SIMPLIFIED_WRITE_SUPPORTED)

#include "constants.h"
//#include "vec2.h"
#include "vec3.h"
//#include "plane.h"
#include "matrix.h"
//#include "cielab.h"
#include "jzazbz.h"
#include "gamutbounds.h"
#include "colormisc.h"

// this has to be global because it's too big for the stack
png_byte memos[256][256][256][4];

int main(int argc, const char **argv){
    printf("hello world\n");
    
    // parameter processing -------------------------------------------------------------------
    
    if ((argc < 2) || (strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "-h") == 0)){
        printf("display help message\n");
        // TODO: make a useful help message
        return 0;
    }
    
    // defaults
    bool filemode = true;
    bool gammamode = true;
    bool softkneemode = true;
    int mapdirection = MAP_GCUSP;
    int mapmode = MAP_COMPRESS;
    int sourcegamutindex = GAMUT_NTSCJ_R;
    int destgamutindex = GAMUT_SRGB;
    int safezonetype = RMZONE_DELTA_BASED;
    char* inputfilename;
    char* outputfilename;
    char* inputcolorstring;
    vec3 inputcolor;
    bool infileset = false;
    bool outfileset = false;
    bool incolorset = false;
    double remapfactor = 0.4;
    double remaplimit = 0.8;
    double kneefactor = 0.2;
    
    int expect = 0;
    for (int i=1; i<argc; i++){
        if (expect == 1){ // expecting input filename
            inputfilename = const_cast<char*>(argv[i]);
            expect  = 0;
            infileset = true;
        }
        else if (expect == 2){ // expecting output filename
            outputfilename = const_cast<char*>(argv[i]);
            expect  = 0;
            outfileset = true;
        }
        else if (expect == 3){ // expecting a color
            inputcolorstring = const_cast<char*>(argv[i]);
            expect  = 0;
            incolorset = true;
        }
        else if (expect == 4){ // expecting source gamut
            if ((strcmp(argv[i], "srgb") == 0)){
                sourcegamutindex = GAMUT_SRGB;
            }
            else if ((strcmp(argv[i], "ntscj") == 0) || (strcmp(argv[i], "ntscjr") == 0)){
                sourcegamutindex = GAMUT_NTSCJ_R;
            }
            else if (strcmp(argv[i], "ntscjb") == 0){
                sourcegamutindex = GAMUT_NTSCJ_B;
            }
            else if (strcmp(argv[i], "smptec") == 0){
                sourcegamutindex = GAMUT_SMPTEC;
            }
            else if (strcmp(argv[i], "ebu") == 0){
                sourcegamutindex = GAMUT_EBU;
            }
            else {
                printf("Invalid parameter for source gamut. Expecting \"srgb\", \"ntscj\", \"ntscjr\", \"ntscjb\", \"smptec\", or \"ebu\".\n");
                return ERROR_BAD_PARAM_SOURCE_GAMUT;
            }
            expect  = 0;
        }
        else if (expect == 5){ // expecting source gamut
            if (strcmp(argv[i], "srgb") == 0){
                destgamutindex = GAMUT_SRGB;
            }
            else if ((strcmp(argv[i], "ntscj") == 0) || (strcmp(argv[i], "ntscjr") == 0)){
                destgamutindex = GAMUT_NTSCJ_R;
            }
            else if (strcmp(argv[i], "ntscjb") == 0){
                destgamutindex = GAMUT_NTSCJ_B;
            }
            else if (strcmp(argv[i], "smptec") == 0){
                destgamutindex = GAMUT_SMPTEC;
            }
            else if (strcmp(argv[i], "ebu") == 0){
                destgamutindex = GAMUT_EBU;
            }
            else {
                printf("Invalid parameter for destination gamut. Expecting \"srgb\", \"ntscj\", \"ntscjr\", \"ntscjb\", \"smptec\", or \"ebu\".\n");
                return ERROR_BAD_PARAM_DEST_GAMUT;
            }
            expect  = 0;
        }
        else if (expect == 6){ // expecting map mode
            if (strcmp(argv[i], "clip") == 0){
                mapmode = MAP_CLIP;
            }
            else if (strcmp(argv[i], "compress") == 0){
                mapmode = MAP_COMPRESS;
            }
            else if (strcmp(argv[i], "expand") == 0){
                mapmode = MAP_EXPAND;
            }
            else {
                printf("Invalid parameter for mapping mode. Expecting \"clip\", \"compress\", or \"expand\".\n");
                return ERROR_BAD_PARAM_MAPPING_MODE;
            }
            expect  = 0;
        }
        else if ((expect == 7) || (expect == 8) || (expect == 9)){
            char* endptr;
            errno = 0; //make sure errno is 0 before strtol()
            double input = strtod(argv[i], &endptr);
            bool inputok = true;
            // are there any chacters left in the input string?
            if (*endptr != '\0'){
                inputok = false;
            }
            // is errno set?
            else if (errno != 0){
                inputok = false;
            }
            if (inputok){
                switch (expect){
                    case 7:
                        remapfactor = input;
                        break;
                    case 8:
                        remaplimit = input;
                        break;
                    case 9:
                        kneefactor = input;
                        break;
                    default:
                        break;
                };
            }
            else {
                printf("Invalid parameter for remap factor, remap limit, or knee factor. (Malformed float.)");
                return ERROR_BAD_PARAM_MAPPING_FLOAT;
            }
            expect = 0;
        }
        else if (expect == 10){ // expecting map direction
            if (strcmp(argv[i], "gcusp") == 0){
                mapdirection = MAP_GCUSP;
            }
            else if (strcmp(argv[i], "hlpcm") == 0){
                mapmode = MAP_HLPCM;
            }
            else {
                printf("Invalid parameter for mapping direction. Expecting \"gcusp\" or \"hlpcm\".\n");
                return ERROR_BAD_PARAM_MAPPING_DIRECTION;
            }
            expect  = 0;
        }
        else if (expect == 11){ // safe zone type
            if (strcmp(argv[i], "const-detail") == 0){
                safezonetype = RMZONE_DELTA_BASED;
            }
            else if (strcmp(argv[i], "const-fidelity") == 0){
                safezonetype = RMZONE_DEST_BASED;
            }
            else {
                printf("Invalid parameter for safe zone type. Expecting \"const-detail\" or \"const-fidelity\".\n");
                return ERROR_BAD_PARAM_ZONE_TYPE;
            }
            expect  = 0;
        }
        else {
            if ((strcmp(argv[i], "--infile") == 0) || (strcmp(argv[i], "-i") == 0)){
                filemode = true;
                expect = 1;
            }
            else if ((strcmp(argv[i], "--outfile") == 0) || (strcmp(argv[i], "-o") == 0)){
                expect = 2;
            }
            else if ((strcmp(argv[i], "--color") == 0) || (strcmp(argv[i], "-c") == 0)){
                filemode = false;
                expect = 3;
            }
            else if ((strcmp(argv[i], "--gamma") == 0) || (strcmp(argv[i], "-g") == 0)){
                gammamode = true;
            }
            else if ((strcmp(argv[i], "--linear") == 0) || (strcmp(argv[i], "-l") == 0)){
                gammamode = false;
            }
            else if ((strcmp(argv[i], "--source-gamut") == 0) || (strcmp(argv[i], "-s") == 0)){
                expect = 4;
            }
            else if ((strcmp(argv[i], "--dest-gamut") == 0) || (strcmp(argv[i], "-d") == 0)){
                expect = 5;
            }
            else if ((strcmp(argv[i], "--map-mode") == 0) || (strcmp(argv[i], "-m") == 0)){
                expect = 6;
            }
            else if ((strcmp(argv[i], "--remap-factor") == 0)/* || (strcmp(argv[i], "-m") == 0)*/){
                expect = 7;
            }
            else if ((strcmp(argv[i], "--remap-limit") == 0)/* || (strcmp(argv[i], "-m") == 0)*/){
                expect = 8;
            }
            else if ((strcmp(argv[i], "--knee-factor") == 0)/* || (strcmp(argv[i], "-m") == 0)*/){
                expect = 9;
            }
            else if ((strcmp(argv[i], "--soft-knee") == 0)/* || (strcmp(argv[i], "-m") == 0)*/){
                softkneemode = true;
            }
            else if ((strcmp(argv[i], "--hard-knee") == 0)/* || (strcmp(argv[i], "-m") == 0)*/){
                softkneemode = false;
            }
            else if ((strcmp(argv[i], "--remap-direction") == 0)/* || (strcmp(argv[i], "-m") == 0)*/){
                expect = 10;
            }
            else if ((strcmp(argv[i], "--safe-zone-type") == 0)/* || (strcmp(argv[i], "-m") == 0)*/){
                expect = 11;
            }
            else {
                printf("Invalid parameter: ||%s||\n", argv[i]);
                return ERROR_BAD_PARAM_UNKNOWN_PARAM;
            }
        }
    }
    
    if (expect > 0){
        printf("Missing parameter. Expecting ");
        switch (expect){
            case 1:
                printf("input file name.\n.");
                break;
            case 2:
                printf("output file name.\n.");
                break;
            case 3:
                printf("input color.\n.");
                break;
            case 4:
                printf("souce gamut name.\n.");
                break;
            case 5:
                printf("destination gamut name.\n.");
                break;
            case 6:
                printf("mapping mode.\n.");
                break;
            case 7:
                printf("remap factor.\n.");
                break;
            case 8:
                printf("remap limit.\n.");
                break;
            case 9:
                printf("knee factor.\n.");
                break;
             case 10:
                printf("remap direction.\n.");
                break;
             case 11:
                printf("safe zone type.\n.");
                break;
            default:
                printf("oh... er... wtf error!.\n.");
        }
        return ERROR_BAD_PARAM_MISSING_VALUE;
    }
    
    if (filemode){
        bool failboat = false;
        if (!infileset){
            printf("Input file not specified.\n");
            failboat = true;
        }
        if (!outfileset){
            printf("Output file not specified.\n");
            failboat = true;
        }
        if (failboat){
            return ERROR_BAD_PARAM_FILE_NOT_SPECIFIED;
        }
    }
    else{
        if (!incolorset){
            printf("Input color not specified.\n");
            return ERROR_BAD_PARAM_COLOR_NOT_SPECIFIED;
        }
        char* endptr;
        errno = 0; //make sure errno is 0 before strtol()
        long int input = strtol(inputcolorstring, &endptr, 0);
        bool inputok = true;
        // did we consume exactly 8 characters?
        if (endptr - inputcolorstring != 8){
            inputok = false;
        }
        // are there any chacters left in the input string?
        else if (*endptr != '\0'){
            inputok = false;
        }
        // is errno set?
        else if (errno != 0){
            inputok = false;
        }
        if (!inputok){
            printf("Invalid input color. Format is  \"0xRRGGBB\" a 0x-prefixed hexadecimal representation of a RGB8 pixel value.\n");
            return ERROR_BAD_PARAM_INVALID_COLOR;
        }
        inputcolor.x = (input >> 16) / 255.0;
        inputcolor.y = ((input & 0x0000FF00) >> 8) / 255.0;
        inputcolor.y = (input & 0x000000FF) / 255.0;
    }
    

    
     //TODO: parse verbose level error 9
     
     // TODO: make the knee function params tunable
    
    //TODO: screen barf params in verbos emode
    
    // TODO: safety check knee paramers don't go below 0
    
    printf("parameter prse complete\n");
    
    
    if (!initializeInverseMatrices()){
        printf("Unable to initialize inverse Jzazbz matrices. WTF error!\n");
        return ERROR_INVERT_MATRIX_FAIL;
    }
    
    //gamutdescriptor::initialize(std::string name, vec3 wp, vec3 rp, vec3 gp, vec3 bp, bool issource, vec3 dwp, int verbose)
    gamutdescriptor sourcegamut;
    //bool srcOK = sourcegamut.initialize("NTSC-J", vec3(0.281, 0.311, 0.408), vec3(0.67, 0.33, 0.0), vec3(0.21, 0.71, 0.08), vec3(0.14, 0.08, 0.78), true, vec3(0.312713, 0.329016, 0.358271), 1);
    vec3 sourcewhite = vec3(gamutpoints[sourcegamutindex][0][0], gamutpoints[sourcegamutindex][0][1], gamutpoints[sourcegamutindex][0][2]);
    vec3 sourcered = vec3(gamutpoints[sourcegamutindex][1][0], gamutpoints[sourcegamutindex][1][1], gamutpoints[sourcegamutindex][1][2]);
    vec3 sourcegreen = vec3(gamutpoints[sourcegamutindex][2][0], gamutpoints[sourcegamutindex][2][1], gamutpoints[sourcegamutindex][2][2]);
    vec3 sourceblue = vec3(gamutpoints[sourcegamutindex][3][0], gamutpoints[sourcegamutindex][3][1], gamutpoints[sourcegamutindex][3][2]);
    // TODO: remove dest whitepoint since it's not used anymore
    bool srcOK = sourcegamut.initialize(gamutnames[sourcegamutindex], sourcewhite, sourcered, sourcegreen, sourceblue, true, D65, 1);
    gamutdescriptor destgamut;
    
    //bool destOK = destgamut.initialize("sRGB", vec3(0.312713, 0.329016, 0.358271), vec3(0.64, 0.33, 0.03), vec3(0.3, 0.6, 0.1), vec3(0.15, 0.06, 0.79), false, vec3(0.312713, 0.329016, 0.358271), 1);
    vec3 destwhite = vec3(gamutpoints[destgamutindex][0][0], gamutpoints[destgamutindex][0][1], gamutpoints[destgamutindex][0][2]);
    vec3 destred = vec3(gamutpoints[destgamutindex][1][0], gamutpoints[destgamutindex][1][1], gamutpoints[destgamutindex][1][2]);
    vec3 destgreen = vec3(gamutpoints[destgamutindex][2][0], gamutpoints[destgamutindex][2][1], gamutpoints[destgamutindex][2][2]);
    vec3 destblue = vec3(gamutpoints[destgamutindex][3][0], gamutpoints[destgamutindex][3][1], gamutpoints[destgamutindex][3][2]);
    bool destOK = destgamut.initialize("sRGB", destwhite, destred, destgreen, destblue, false, D65, 1);
    if (! srcOK || !destOK){
        printf("Gamut descriptor initializtion failed. All is lost. Abandon ship.\n");
        return 101;
    }

    
    if (!filemode){
        int redout;
        int greenout;
        int blueout;
        vec3 linearinputcolor = (gammamode) ? vec3(tolinear(inputcolor.x), tolinear(inputcolor.y), tolinear(inputcolor.z)) : inputcolor;
        vec3 outcolor;
        
        if (mapmode == MAP_CLIP){
            vec3 tempcolor = sourcegamut.linearRGBtoXYZ(linearinputcolor);
            outcolor = destgamut.XYZtoLinearRGB(tempcolor);
        }
        else {
            outcolor = mapColor(linearinputcolor, sourcegamut, destgamut, (mapmode == MAP_EXPAND), remapfactor, remaplimit, softkneemode, kneefactor, mapdirection, safezonetype);
        }
        if (gammamode){
            outcolor = vec3(togamma(outcolor.x), togamma(outcolor.y), togamma(outcolor.z));
        }
        redout = toRGB8nodither(outcolor.x);
        greenout = toRGB8nodither(outcolor.y);
        blueout = toRGB8nodither(outcolor.z);
        printf("Input color %s converts to 0x%02X%02X%02X (red: %i, green: %i, blue: %i)\n", inputcolorstring, redout, greenout, blueout, redout, greenout, blueout);
        return 0;
    }

    int result = 1000;
    png_image image;

    
    /* Only the image structure version number needs to be set. */
    memset(&image, 0, sizeof image);
    image.version = PNG_IMAGE_VERSION;

    if (png_image_begin_read_from_file(&image, inputfilename)){
        png_bytep buffer;

        /* Change this to try different formats!  If you set a colormap format
        * then you must also supply a colormap below.
        */
        image.format = PNG_FORMAT_RGBA;

        buffer = (png_bytep) malloc(PNG_IMAGE_SIZE(image));  //c++ wants an explict cast

        if (buffer != NULL){
        if (png_image_finish_read(&image, NULL/*background*/, buffer, 0/*row_stride*/, NULL/*colormap for PNG_FORMAT_FLAG_COLORMAP */)){
            
            // ------------------------------------------------------------------------------------------------------------------------------------------
            // Begin actual color conversion code
            
            
            //png_byte memos[256][256][256][4];
            memset(&memos, 0, 256 * 256 * 256 * 4 * sizeof(png_byte));
            
            int width = image.width;
            int height = image.height;
            for (int y=0; y<height; y++){
                printf("row %i of %i...\n", y+1, height);
                for (int x=0; x<width; x++){
                    
                    
                    png_byte redin = buffer[ ((y * width) + x) * 4];
                    png_byte greenin = buffer[ (((y * width) + x) * 4) + 1 ];
                    png_byte bluein = buffer[ (((y * width) + x) * 4) + 2 ];
                    
                    png_byte redout, greenout, blueout;
                    
                    if (memos[redin][greenin][bluein][0] == 1){
                        redout = memos[redin][greenin][bluein][1];
                        greenout = memos[redin][greenin][bluein][2];
                        blueout = memos[redin][greenin][bluein][3];
                    }
                    else {
                    
                        // read out from buffer and convert to double
                        //double redvalue = buffer[ ((y * width) + x) * 4]/255.0;
                        //double greenvalue = buffer[ (((y * width) + x) * 4) + 1 ]/255.0;
                        //double bluevalue = buffer[ (((y * width) + x) * 4) + 2 ]/255.0;
                        double redvalue = redin/255.0;
                        double greenvalue = greenin/255.0;
                        double bluevalue = bluein/255.0;
                        // don't touch alpha value
                        
                        // to linear RGB
                        if (gammamode){
                            redvalue = tolinear(redvalue);
                            greenvalue = tolinear(greenvalue);
                            bluevalue = tolinear(bluevalue);
                        }
                        // The FF7 videos had banding near black when decoded with any piecewise "toe slope" gamma function, suggesting that a pure curve function was needed. May need to try this if such banding appears.
                        //redvalue = clampdouble(pow(redvalue, 2.2));
                        //greenvalue = clampdouble(pow(greenvalue, 2.2));
                        //bluevalue = clampdouble(pow(bluevalue, 2.2));
                        
                        
                        vec3 outcolor;
                        vec3 linearRGB = vec3(redvalue, greenvalue, bluevalue);
                        
                        if (mapmode == MAP_CLIP){
                            vec3 tempcolor = sourcegamut.linearRGBtoXYZ(linearRGB);
                            outcolor = destgamut.XYZtoLinearRGB(tempcolor);
                        }
                        else {
                            outcolor = mapColor(linearRGB, sourcegamut, destgamut, (mapmode == MAP_EXPAND), remapfactor, remaplimit, softkneemode, kneefactor, mapdirection, safezonetype);
                        }
                        
                        // back to sRGB
                        if (gammamode){
                            outcolor = vec3(togamma(outcolor.x), togamma(outcolor.y), togamma(outcolor.z));
                        }
                        
                        // back to png_byte
                        redout = toRGB8nodither(outcolor.x);
                        greenout = toRGB8nodither(outcolor.y);
                        blueout = toRGB8nodither(outcolor.z);
                        
                        // memoize
                        memos[redin][greenin][bluein][0] = 1;
                        memos[redin][greenin][bluein][1] = redout;
                        memos[redin][greenin][bluein][2] = greenout;
                        memos[redin][greenin][bluein][3] = blueout;
                    
                    }

                    buffer[ ((y * width) + x) * 4] = redout;
                    buffer[ (((y * width) + x) * 4) + 1 ] = greenout;
                    buffer[ (((y * width) + x) * 4) + 2 ] = blueout;
                                            
                }
            }
            
            // End actual color conversion code
            // ------------------------------------------------------------------------------------------------------------------------------------------
            
            
            if (png_image_write_to_file(&image, outputfilename, 0/*convert_to_8bit*/, buffer, 0/*row_stride*/, NULL/*colormap*/)){
                result = 0;
                printf("done.\n");
            }

            else {
                fprintf(stderr, "ntscjpng: write %s: %s\n", outputfilename, image.message);
            }
        }

        else {
            fprintf(stderr, "ntscjpng: read %s: %s\n", inputfilename, image.message);
        }

        free(buffer);
        buffer = NULL;
        }

        else {
        fprintf(stderr, "ntscjpng: out of memory: %lu bytes\n", (unsigned long)PNG_IMAGE_SIZE(image));

        /* This is the only place where a 'free' is required; libpng does
            * the cleanup on error and success, but in this case we couldn't
            * complete the read because of running out of memory and so libpng
            * has not got to the point where it can do cleanup.
            */
        png_image_free(&image);
        }
    }

    else {
        /* Failed to read the input file argument: */
        fprintf(stderr, "ntscjpng: %s: %s\n", inputfilename, image.message);
    }
   

   //else {
      /* Wrong number of arguments */
      //fprintf(stderr, "ntscjpng: usage: ntscjpng mode bounds-mode input-file output-file, where mode is \"ntscj-to-srgb\" or \"srgb-to-ntscj\" and bounds-mode is \"clip,\" \"compress,\" or \"scale.\" \n");
   //}

   return result;
}
#endif
