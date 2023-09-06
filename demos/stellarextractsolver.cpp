// An executable to solve-plate images (determine RA/DEC location from visible stars)
//
// Build with:
// mkdir build
// cd build
// cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTER=OFF -DBUILD_DEMOS=ON .. 
// make -j 4
//
// Get index files from <http://data.astrometry.net/>
// or install     astrometry-data-tycho2 (300 MB)
// and optionally astrometry-data-2mass (39 GB)

// Qapp required to synchronize threads
#include <QApplication>
#include <time.h>

//Includes for this project
#include "structuredefinitions.h"
#include "stellarsolver.h"
#include "ssolverutils/fileio.h"

// support for RAW files (libraw)
// see https://github.com/mardy/qtraw/blob/master/tests/qtraw-test.cpp
// see https://github.com/FMeinicke/QtRaw

void print_usage(char *pgmname)
{ /* Show program help. pgmname = argv[0] */
  printf("Usage: %s [options] img1 img2 ...\n", pgmname);
  printf("This tool determines RA/DEC location from visible stars (solve-plate)\n");
  printf("Version: %s\n", StellarSolver_BUILD_TS);
  printf("The image files can be FITS, JPG, PNG, TIFF, BMP, SVG\n");
  printf("Options:\n");
  printf("-Idir         Add 'dir' to the list of locations holding Astrometry star\n");
  printf("                catalog index files. Any defined environment variable\n");
  printf("                ASTROMETRY_INDEX_FILES path will also be added.\n"); 
  printf("-o FILE       Use FILE for the output. Default is write to stdout\n");
  printf("--overwrite   Overwrite output files if they already exist\n");
  printf("--skip-solved Skip  input files for which the 'solved' output file already exists\n");
  printf("--help -h     Display this help and version.\n");
  printf("--verbose     Display detailed processing steps.\n");
  printf("--silent      Quiet mode.\n");
  printf("The program return-code is the number of actually processed images.\n");
  exit(0);
} // print_usage

// from stellarsolver.cpp
void addPathToListIfExists(QStringList *list, QString path)
{
    if(list && QFileInfo::exists(path)) list->append(path);
}

int main(int argc, char *argv[])
{
    int         i;
    int         imageFilesCounter =0;
    int         flagVerbosity     =1;  // 0:silent 1:normal 2:verbose
    int         flagOverwrite     =0;
    int         flagFileOpen      =0;
    int         flagSkipSolved    =0;
    char*       outputFilename    =NULL;
    QStringList catalogDirectories;
    QStringList imageFiles;
    
    QApplication app(argc, argv); // create threads
    
    catalogDirectories = StellarSolver::getDefaultIndexFolderPaths();
    
    // Parse options -----------------------------------------------------------
    clock_t t=clock();
    for (i = 1; i < argc; i++)
    {
      if (!strncmp("-I", argv[i], 2)) { // add a directory to hold star catalog index files
        if (strlen(argv[i]) > 2) addPathToListIfExists(&catalogDirectories, QString(argv[i]+2));
        else if (i+1 < argc) {
          addPathToListIfExists(&catalogDirectories, QString(argv[i++]));
        }
      } else if ((!strcmp("-o", argv[i]) || !strcmp("--out", argv[i])) 
                  && i+1 < argc) { // add an output file name
        outputFilename = argv[i++];
      } else if (!strcmp("--verbose", argv[i])) {  
        flagVerbosity = 2;
      } else if (!strcmp("--silent", argv[i])) {  
        flagVerbosity = 0;
      } else if (!strcmp("--overwrite", argv[i])) {
        flagOverwrite = 1;
      } else if (!strcmp("--skip-solved", argv[i])) {
        flagSkipSolved = 1;
      } else if (argv[i][0] != '-')  {
        // does not start with '-' -> add a file to process
        addPathToListIfExists(&imageFiles, QString(argv[i]));
      } else print_usage(argv[0]);  
    }
    // Add env  ASTROMETRY_INDEX_FILES
    if (getenv("ASTROMETRY_INDEX_FILES"))
      addPathToListIfExists(&catalogDirectories, QString(getenv("ASTROMETRY_INDEX_FILES")));
    
#if defined(__linux__)
    setlocale(LC_NUMERIC, "C");
#endif
    fileio imageLoader;
    
    if (flagVerbosity >= 2) {
      printf("%s %s\n", argv[0], StellarSolver_BUILD_TS);
      printf("INFO: Star Catalog Index Path:\n");
      for(int i = 0; i < catalogDirectories.count(); i++) {
        const QString &currentDir = catalogDirectories[i];
        printf("- %s\n", currentDir.toStdString().c_str());
      }
    }
    
    // loop on images ----------------------------------------------------------
    for(int i = 0; i < imageFiles.count(); i++)
    {
      #define BUFFER_SIZE 65535
      const QString &currentImage = imageFiles[i];
      char  currentImageName[BUFFER_SIZE];
      char  outputFilenameImage[BUFFER_SIZE];
      int   count;
      
      strncpy(currentImageName, currentImage.toStdString().c_str(), BUFFER_SIZE);
      count = snprintf(outputFilenameImage, BUFFER_SIZE, "%s.yaml", currentImageName);
      if (flagVerbosity >= 1)
        printf("INFO: Loading image     %s\n", currentImageName);
      if (flagSkipSolved && count && QFileInfo::exists(QString(outputFilenameImage)) && flagVerbosity >= 2) {
        printf("INFO: Already solved    %s\n", currentImageName);
        continue;
      }
      if(!imageLoader.loadImage(currentImage))
      {
          fprintf(stderr, "ERROR: Can not load image %s\n", currentImageName);
          continue;
      }
      FITSImage::Statistic stats = imageLoader.getStats();
      uint8_t *imageBuffer = imageLoader.getImageBuffer();
      
      StellarSolver stellarSolver(stats, imageBuffer);
      stellarSolver.setIndexFolderPaths(catalogDirectories);

      if (flagVerbosity >= 2)
        printf("INFO: Starting to solve %s\n", currentImageName);
      fflush( stdout );

      if(!stellarSolver.solve())
      {
          fprintf(stderr, "ERROR: Plate-Solve failed %s\n", currentImageName);
          continue;
      }

      FITSImage::Solution solution = stellarSolver.getSolution();
      
      FILE* outputFilehandle = NULL;
      
      if (!outputFilename) {
        // default: write a YAML file per image based on its name
        if (count)
          outputFilehandle = fopen(outputFilenameImage, flagOverwrite ? "w" : "a");
        if (outputFilehandle)
          flagFileOpen=1;
      } else if (strlen(outputFilename)) {
        // output file name given
        if (!strcmp(outputFilename, "stdout"))
          outputFilehandle = stdout;
        else if (!strcmp(outputFilename, "stderr"))
          outputFilehandle = stderr;
        else {
          outputFilehandle = fopen(outputFilename, flagOverwrite ? "w" : "a");
          if (outputFilehandle) flagFileOpen=1;
        }
      }
      if (!outputFilehandle) // when all failed, output to stdout
        outputFilehandle = stdout;
      
      fprintf(outputFilehandle, "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

      fprintf(outputFilehandle, "Field center: (RA,Dec) = (%f, %f) deg.\n", solution.ra, solution.dec);
      fprintf(outputFilehandle, "Field size: %f x %f arcminutes\n", solution.fieldWidth, solution.fieldHeight);
      fprintf(outputFilehandle, "Pixel Scale: %f\"\n", solution.pixscale);
      fprintf(outputFilehandle, "Field rotation angle: up is %f degrees E of N\n", solution.orientation);
      fprintf(outputFilehandle, "Field parity: %s\n", FITSImage::getParityText(solution.parity).toUtf8().data());
      fflush( outputFilehandle );
      imageFilesCounter++;

      stellarSolver.setParameterProfile(SSolver::Parameters::ALL_STARS);

      if(!stellarSolver.extract(true))
      {
          fprintf(stderr, "ERROR: Solver Star Extraction failed %s", currentImageName);
          continue;
      }

      QList<FITSImage::Star> starList = stellarSolver.getStarList();
      fprintf(outputFilehandle, "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
      
      fprintf(outputFilehandle, "Stars found: %u\n", starList.count());
      if (flagVerbosity >= 2)
        printf("INFO: Stars found in    %s: %u\n", currentImageName, starList.count());
      for(int i=0; i < starList.count(); i++)
      {
          FITSImage::Star star = starList.at(i);
          char *ra = StellarSolver::raString(star.ra).toUtf8().data();
          char *dec = StellarSolver::decString(star.dec).toUtf8().data();
          fprintf(outputFilehandle, "Star #%u: (%f x, %f y), (ra: %s,dec: %s), mag: %f, peak: %f, hfr: %f \n", i, star.x, star.y, ra , dec, star.mag, star.peak, star.HFR);
      }
      if (flagFileOpen) fclose(outputFilehandle);
      
    } // loop on images
    if (flagVerbosity >= 1) {
      double time_taken = (double)(clock() - t)/CLOCKS_PER_SEC; // calculate the elapsed time
      printf("INFO: Processed images: %u\n", imageFilesCounter);
      printf("INFO: Time elapsed:     %f [s]\n", time_taken);
      if (imageFilesCounter>1) 
        printf("INFO: Time elapsed per image: %f [s]\n", time_taken/imageFilesCounter );
    }
    return imageFilesCounter;
}
