#include <string.h>
#include <stdio.h>
#include <string>
#include <errno.h>

// Include either installed libpng or local copy. Linux should have libpng-dev installed; Windows users can figure stuff out.
//#include "../../png.h"
#include <png.h> 

#include "constants.h"
#include "vec3.h"
#include "matrix.h"
//#include "cielab.h"
#include "jzazbz.h"
#include "gamutbounds.h"
#include "colormisc.h"

typedef struct memo{
    bool known;
    vec3 data;
} memo;
    
// this has to be global because it's too big for the stack
memo memos[256][256][256];

int main(int argc, const char **argv){
    
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
    bool dither = false;
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
    int verbosity = VERBOSITY_SLIGHT;
    
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
            else if (strcmp(argv[i], "vp") == 0){
                mapmode = MAP_VP;
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
        else if (expect == 12){ // knee type
            if (strcmp(argv[i], "hard") == 0){
                softkneemode = false;
            }
            else if (strcmp(argv[i], "soft") == 0){
                softkneemode = true;
            }
            else {
                printf("Invalid parameter for knee type. Expecting \"soft\" or \"hard\".\n");
                return ERROR_BAD_PARAM_KNEE_TYPE;
            }
            expect  = 0;
        }
        else if (expect == 13){ // gamma type
            if (strcmp(argv[i], "srgb") == 0){
                gammamode = true;
            }
            else if (strcmp(argv[i], "linear") == 0){
                gammamode = false;
            }
            else {
                printf("Invalid parameter for gamma function. Expecting \"srgb\" or \"linear\".\n");
                return ERROR_BAD_PARAM_GAMMA_TYPE;
            }
            expect  = 0;
        }
        else if (expect == 14){ // dither
            if (strcmp(argv[i], "true") == 0){
                dither = true;
            }
            else if (strcmp(argv[i], "false") == 0){
                dither = false;
            }
            else {
                printf("Invalid parameter for dither. Expecting \"true\" or \"false\".\n");
                return ERROR_BAD_PARAM_DITHER;
            }
            expect  = 0;
        }
        else if (expect == 15){ //verbosity
            char* endptr;
            errno = 0; //make sure errno is 0 before strtol()
            long int input = strtol(argv[i], &endptr, 0);
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
                if (input < VERBOSITY_SILENT){
                    input = VERBOSITY_SILENT;
                }
                if (input > VERBOSITY_EXTREME){
                    input = VERBOSITY_EXTREME;
                }
                verbosity = input;
            }
            else {
                printf("Invalid parameter for verbosity. (Malformed int.)");
                return ERROR_BAD_PARAM_VERBOSITY;
            }
            expect = 0;
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
                expect = 13;
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
            else if ((strcmp(argv[i], "--remap-factor") == 0) || (strcmp(argv[i], "--rf") == 0)){
                expect = 7;
            }
            else if ((strcmp(argv[i], "--remap-limit") == 0) || (strcmp(argv[i], "--rl") == 0)){
                expect = 8;
            }
            else if ((strcmp(argv[i], "--knee-factor") == 0) || (strcmp(argv[i], "--kf") == 0)){
                expect = 9;
            }
            else if ((strcmp(argv[i], "--knee") == 0) || (strcmp(argv[i], "-k") == 0)){
                expect = 12;
            }
            else if ((strcmp(argv[i], "--gamut-mapping-algorithm") == 0) || (strcmp(argv[i], "--gma") == 0)){
                expect = 10;
            }
            else if ((strcmp(argv[i], "--safe-zone-type") == 0) || (strcmp(argv[i], "-z") == 0)){
                expect = 11;
            }
            else if ((strcmp(argv[i], "--dither") == 0) || (strcmp(argv[i], "--di") == 0)){
                expect = 14;
            }
            else if ((strcmp(argv[i], "--verbosity") == 0) || (strcmp(argv[i], "-v") == 0)){
                expect = 15;
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
            case 12:
                printf("knee type.\n.");
                break;
            case 13:
                printf("gamma function.\n.");
                break;
            case 14:
                printf("dither setting.\n.");
                break;
            case 15:
                printf("verbosity level.\n.");
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
        inputcolor.z = (input & 0x000000FF) / 255.0;
    }
    
    //TODO: screen barf params in verbos emode
    if (verbosity >= VERBOSITY_SLIGHT){
        printf("\n\n----------\nParameters are:\n");
        if (filemode){
            printf("Input file: %s\nOutput file: %s\n", inputfilename, outputfilename);
        }
        else {
            printf("Input color: %s\n", inputcolorstring);
        }
        if (gammamode){
            printf("Gamma function: srgb\n");
        }
        else {
            printf("Gamma function: linear\n");
        }
        printf("Source gamut: ");
        switch(sourcegamutindex){
            case GAMUT_SRGB:
                printf("srgb\n");
                break;
            case GAMUT_NTSCJ_R:
                printf("ntscjr (ntscj)\n");
                break;
            case GAMUT_NTSCJ_B:
                printf("ntscjb\n");
                break;
            case GAMUT_SMPTEC:
                printf("smptec\n");
                break;
            case GAMUT_EBU:
                printf("ebu\n");
                break;
            default:
                break;
        };
        printf("Destination gamut: ");
        switch(destgamutindex){
            case GAMUT_SRGB:
                printf("srgb\n");
                break;
            case GAMUT_NTSCJ_R:
                printf("ntscjr (ntscj)\n");
                break;
            case GAMUT_NTSCJ_B:
                printf("ntscjb\n");
                break;
            case GAMUT_SMPTEC:
                printf("smptec\n");
                break;
            case GAMUT_EBU:
                printf("ebu\n");
                break;
            default:
                break;
        };
        printf("Gamut mapping mode: ");
        switch(mapmode){
            case MAP_CLIP:
                printf("clip\n");
                break;
            case MAP_COMPRESS:
                printf("compress\n");
                break;
            case MAP_EXPAND:
                printf("expand\n");
                break;
            default:
                break;
        };
        if (mapmode > MAP_CLIP){
            printf("Gamut mapping algorithm: ");
            switch(mapdirection){
                case MAP_GCUSP:
                    printf("gcusp\n");
                    break;
                case MAP_HLPCM:
                    printf("hlpcm\n");
                    break;
                case MAP_VP:
                    printf("vp\n");
                    break;
                default:
                    break;
            };
            printf("Safe zone type: ");
            switch(safezonetype){
                case RMZONE_DELTA_BASED:
                    printf("const-detail\n");
                    break;
                case RMZONE_DEST_BASED:
                    printf("const-fidelity\n");
                    break;
                default:
                    break;
            };
            if (safezonetype == RMZONE_DELTA_BASED){
                printf("Remap Factor: %f\n", remapfactor);
            }
            printf("Remap Limit: %f\n", remaplimit);
            if (softkneemode){
                printf("Knee type: soft\nKnee factor: %f\n", kneefactor);
            }
            else{
                printf("Knee type: hard\n");
            }
        }
        if (filemode){
            if (dither){
                printf("Dither: true\n");
            }
            else {
                printf("Dither: false\n");
            }
        }
        printf("Verbosity: %i\n", verbosity);
        printf("----------\n\n");
    }
    
    
    if (!initializeInverseMatrices()){
        printf("Unable to initialize inverse Jzazbz matrices. WTF error!\n");
        return ERROR_INVERT_MATRIX_FAIL;
    }
    
    gamutdescriptor sourcegamut;
    vec3 sourcewhite = vec3(gamutpoints[sourcegamutindex][0][0], gamutpoints[sourcegamutindex][0][1], gamutpoints[sourcegamutindex][0][2]);
    vec3 sourcered = vec3(gamutpoints[sourcegamutindex][1][0], gamutpoints[sourcegamutindex][1][1], gamutpoints[sourcegamutindex][1][2]);
    vec3 sourcegreen = vec3(gamutpoints[sourcegamutindex][2][0], gamutpoints[sourcegamutindex][2][1], gamutpoints[sourcegamutindex][2][2]);
    vec3 sourceblue = vec3(gamutpoints[sourcegamutindex][3][0], gamutpoints[sourcegamutindex][3][1], gamutpoints[sourcegamutindex][3][2]);
    // TODO: remove dest whitepoint since it's not used anymore
    bool srcOK = sourcegamut.initialize(gamutnames[sourcegamutindex], sourcewhite, sourcered, sourcegreen, sourceblue, true, verbosity);
    gamutdescriptor destgamut;
    
    vec3 destwhite = vec3(gamutpoints[destgamutindex][0][0], gamutpoints[destgamutindex][0][1], gamutpoints[destgamutindex][0][2]);
    vec3 destred = vec3(gamutpoints[destgamutindex][1][0], gamutpoints[destgamutindex][1][1], gamutpoints[destgamutindex][1][2]);
    vec3 destgreen = vec3(gamutpoints[destgamutindex][2][0], gamutpoints[destgamutindex][2][1], gamutpoints[destgamutindex][2][2]);
    vec3 destblue = vec3(gamutpoints[destgamutindex][3][0], gamutpoints[destgamutindex][3][1], gamutpoints[destgamutindex][3][2]);
    bool destOK = destgamut.initialize("sRGB", destwhite, destred, destgreen, destblue, false, verbosity);
    
    if (! srcOK || !destOK){
        printf("Gamut descriptor initializtion failed. All is lost. Abandon ship.\n");
        return GAMUT_INITIALIZE_FAIL;
    }

    // this mode converts a single color and printfs the result
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
        if (verbosity >= VERBOSITY_MINIMAL){
            printf("Input color %s (red %f, green %f, blue %f; red %i, green %i, blue %i) converts to 0x%02X%02X%02X (red %f, green %f, blue %f; red: %i, green: %i, blue: %i)\n", inputcolorstring, inputcolor.x, inputcolor.y, inputcolor.z,  toRGB8nodither(inputcolor.x), toRGB8nodither(inputcolor.y), toRGB8nodither(inputcolor.z), redout, greenout, blueout, outcolor.x, outcolor.y, outcolor.z, redout, greenout, blueout);
        }
        else {
            printf("%02X%02X%02X", redout, greenout, blueout);
        }
        return RETURN_SUCCESS;
    }

    // if we didn't just return, we are in file mode
    
    int result = ERROR_PNG_FAIL;
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
                
                // zero the memos
                memset(&memos, 0, 256 * 256 * 256 * sizeof(memo));
                
                if (verbosity >= VERBOSITY_MINIMAL){
                    printf("Doing gamut conversion on %s and saving result to %s...\n", inputfilename, outputfilename); 
                }
                
                // iterate over every pixel
                int width = image.width;
                int height = image.height;
                for (int y=0; y<height; y++){
                    if (verbosity >= VERBOSITY_HIGH){
                        printf("\trow %i of %i...\n", y+1, height);
                    }
                    else if (verbosity >= VERBOSITY_MINIMAL){
                        if (y == 0){
                            printf("0%%... ");
                            fflush(stdout);
                        }
                        else if ((y < height -1) && ((((y+1)*20)/height) > ((y*20)/height))){
                            printf("%i%%... ", ((y+1)*100)/height);
                            if (((y+1)*100)/height == 50){
                                printf("\n");
                            }
                            fflush(stdout);
                        }
                    }
                    for (int x=0; x<width; x++){
                        
                        // read bytes from buffer
                        png_byte redin = buffer[ ((y * width) + x) * 4];
                        png_byte greenin = buffer[ (((y * width) + x) * 4) + 1 ];
                        png_byte bluein = buffer[ (((y * width) + x) * 4) + 2 ];
                        
                        vec3 outcolor;
                        
                        // if we've already processed the same input color, just recall the memo
                        if (memos[redin][greenin][bluein].known){
                            outcolor = memos[redin][greenin][bluein].data;
                        }
                        else {
                        
                            // convert to double
                            double redvalue = redin/255.0;
                            double greenvalue = greenin/255.0;
                            double bluevalue = bluein/255.0;
                            // don't touch alpha value
                            
                            // to linear RGB
                            if (gammamode){
                                // The FF7 videos had banding near black when decoded with any piecewise "toe slope" gamma function, suggesting that a pure curve function was needed. May need to try this if such banding appears.
                                redvalue = tolinear(redvalue);
                                greenvalue = tolinear(greenvalue);
                                bluevalue = tolinear(bluevalue);
                            }                           
                            
                            vec3 linearRGB = vec3(redvalue, greenvalue, bluevalue);
                            
                            // if clipping, just do the matrix math
                            // (the gamutdescriptors know if they need to do a Bradford (Von Kries) transform
                            if (mapmode == MAP_CLIP){
                                vec3 tempcolor = sourcegamut.linearRGBtoXYZ(linearRGB);
                                outcolor = destgamut.XYZtoLinearRGB(tempcolor);
                            }
                            // otherwise fire up the gamut mapping algorithm
                            else {
                                outcolor = mapColor(linearRGB, sourcegamut, destgamut, (mapmode == MAP_EXPAND), remapfactor, remaplimit, softkneemode, kneefactor, mapdirection, safezonetype);
                            }
                            
                            // back to sRGB
                            if (gammamode){
                                outcolor = vec3(togamma(outcolor.x), togamma(outcolor.y), togamma(outcolor.z));
                            }
                            
                            // memoize the result of the conversion so we don't need to do it again for this input color
                            memos[redin][greenin][bluein].known = true;
                            memos[redin][greenin][bluein].data = outcolor;
                                                    
                        }

                        png_byte redout, greenout, blueout;
                        
                        // dither and back to RGB8 if enabled
                        if (dither){
                            // use inverse x coord for red and inverse y coord for blue to decouple dither patterns across channels
                            // see https://blog.kaetemi.be/2015/04/01/practical-bayer-dithering/
                            redout = quasirandomdither(outcolor.x, width - x - 1, y);
                            greenout = quasirandomdither(outcolor.y, x, y);
                            blueout = quasirandomdither(outcolor.z, x, height - y - 1);
                        }
                        // otherwise just back to RGB 8
                        else {
                            redout = toRGB8nodither(outcolor.x);
                            greenout = toRGB8nodither(outcolor.y);
                            blueout = toRGB8nodither(outcolor.z);
                        }
                        
                        // save back to buffer
                        buffer[ ((y * width) + x) * 4] = redout;
                        buffer[ (((y * width) + x) * 4) + 1 ] = greenout;
                        buffer[ (((y * width) + x) * 4) + 2 ] = blueout;
                                                
                    }
                }
                if ((verbosity >= VERBOSITY_MINIMAL) && (verbosity < VERBOSITY_HIGH)){
                    printf("100%%\n");
                }
                
                // End actual color conversion code
                // ------------------------------------------------------------------------------------------------------------------------------------------
                
                
                if (png_image_write_to_file(&image, outputfilename, 0/*convert_to_8bit*/, buffer, 0/*row_stride*/, NULL/*colormap*/)){
                    result = RETURN_SUCCESS;
                    printf("done.\n");
                }

                else {
                    fprintf(stderr, "ntscjpng: write %s: %s\n", outputfilename, image.message);
                    result = ERROR_PNG_WRITE_FAIL;
                }
            }

            else {
                fprintf(stderr, "ntscjpng: read %s: %s\n", inputfilename, image.message);
                result = ERROR_PNG_READ_FAIL;
            }

            free(buffer);
            buffer = NULL;
        }

        else {
            fprintf(stderr, "ntscjpng: out of memory: %lu bytes\n", (unsigned long)PNG_IMAGE_SIZE(image));
            result = ERROR_PNG_MEM_FAIL;

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
        result = ERROR_PNG_OPEN_FAIL;
    }
   
   return result;
}
