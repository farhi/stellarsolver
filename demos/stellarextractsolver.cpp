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
  printf("This tool determines RA/DEC location from visible stars in images (solve-plate).\n");
  printf("Version: %s\n", StellarSolver_BUILD_TS);
  printf("The image files can be FITS, JPG, PNG, TIFF, BMP, SVG\n");
  printf("Options:\n");
  printf("  -I<dir> | -I <dir> | -d<dir> | -d <dir>\n");
  printf("      Add 'dir' to the list of locations holding Astrometry star\n");
  printf("      catalog index files. Any defined environment variable\n");
  printf("      ASTROMETRY_INDEX_FILES path will also be added.\n"); 
  printf("  --scale-low  | -L <scale_deg>\n");
  printf("  --scale-high | -H <scale_deg>\n");
  printf("      Lower and Upper bound of image scale estimate, width in degrees.\n");
  printf("  --out | -o FILE\n");
  printf("      Use FILE for the output (TOML format). You may use 'stdout'.\n");
  printf("      Default is write a TOML file per image.\n");
  printf("  --overwrite | -O\n");
  printf("      Overwrite output files if they already exist.\n");
  printf("  --ra  <degrees>\n");
  printf("  --dec <degrees>\n");
  printf("      Only search in indexes around field center given by\n");
  printf("      'ra' and 'dec' in degrees.\n");
  printf("  --skip-solved | -J | -K | --continue\n");
  printf("      Skip  input files for which the 'solved' output file already exists.\n");
  printf("  --help | -h\n");
  printf("      Display this help and version.\n");
  printf("  --verbose\n");
  printf("      Display detailed processing steps.\n");
  printf("  --silent\n");
  printf("      Quiet mode.\n");
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
    double      setRA             =NAN;
    double      setDEC            =NAN;
    double      setScaleLow       =NAN;
    double      setScaleHigh      =NAN;
    QStringList catalogDirectories;
    QStringList imageFiles;
    
    QApplication app(argc, argv); // create threads
    
    catalogDirectories = StellarSolver::getDefaultIndexFolderPaths();
    
    // Parse options -----------------------------------------------------------
    clock_t t=clock();
    for (i = 1; i < argc; i++)
    {
      if (!strncmp("-I", argv[i], 2) || !strncmp("-d", argv[i], 2)) { // add a directory to hold star catalog index files
        if (strlen(argv[i]) > 2)  // -Idir
          addPathToListIfExists(&catalogDirectories, QString(argv[i]+2));
        else if (i+1 < argc) {    // -I dir
          addPathToListIfExists(&catalogDirectories, QString(argv[i++]));
        }
      } else if ((!strcmp("--out", argv[i]) || !strncmp("-o", argv[i], 3)) 
                  && i+1 < argc) { // add an output file name
        outputFilename = argv[i++];
      } else if (!strcmp("--ra", argv[i]) && i+1 < argc) { 
        setRA         = strtod(argv[i++], NULL); // set RA  in deg
      } else if (!strcmp("--dec", argv[i]) && i+1 < argc) { 
        setDEC        = strtod(argv[i++], NULL); // set DEC in deg
      } else if (!strcmp("--overwrite",   argv[i]) || !strcmp("-O", argv[i])) {
        flagOverwrite = 1;
      } else if (!strcmp("--scale-low", argv[i])   || !strcmp("-L", argv[i])) {
        setScaleLow   = strtod(argv[i++], NULL);
      } else if (!strcmp("--scale-high", argv[i])  || !strcmp("-H", argv[i])) {
        setScaleHigh  = strtod(argv[i++], NULL);
      } else if (!strcmp("--skip-solved", argv[i]) || !strcmp("-K", argv[i]) 
              || !strcmp("--continue",    argv[i]) || !strcmp("-J", argv[i])) {
        flagSkipSolved= 1;
      } else if (!strcmp("--verbose",     argv[i])) {  
        flagVerbosity = 2;
      } else if (!strcmp("--silent",      argv[i])) {  
        flagVerbosity = 0;
      } else if (argv[i][0] != '-')  {
        // does not start with '-' -> add a file to process
        addPathToListIfExists(&imageFiles, QString(argv[i]));
      } else {
        print_usage(argv[0]);  
      }
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
      count = snprintf(outputFilenameImage, BUFFER_SIZE, "%s.toml", currentImageName);
      if (flagVerbosity >= 1)
        printf("INFO: Loading image     %s [%i/%i]\n", currentImageName, i, imageFiles.count());
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
      
      if (!imageLoader.position_given && !isnan(setRA) && !isnan(setDEC)) {
        imageLoader.position_given = 1;
        imageLoader.ra  = setRA/15.0;   // in hours
        imageLoader.dec = setDEC;       // in deg
      }
      
      if(imageLoader.position_given)
      {
        if (flagVerbosity >= 1)
          printf("INFO: Using Position    %s [RA=%f hours, DEC=%f degrees]\n", 
            currentImageName, imageLoader.ra, imageLoader.dec);
        stellarSolver.setSearchPositionInDegrees(imageLoader.ra, imageLoader.dec);
      }
      
      if (imageLoader.scale_given && !isnan(setScaleLow) && !isnan(setScaleHigh)) {
         imageLoader.scale_given = 1;
         imageLoader.scale_low   = setScaleLow;
         imageLoader.scale_high  = setScaleHigh;
         imageLoader.scale_units = DEG_WIDTH;
      }
      if(imageLoader.scale_given)
      {
          stellarSolver.setSearchScale(imageLoader.scale_low, imageLoader.scale_high, imageLoader.scale_units);
          if (flagVerbosity >= 1)
            printf("INFO  Using Scale       %s [%f to %f, %s]\n", 
              currentImageName, imageLoader.scale_low, imageLoader.scale_high, SSolver::getScaleUnitString(imageLoader.scale_units).toUtf8().data());
      }
      
      // solve options
      // see demosignalsslots: --ra --dec --scale-high --scale-low  .setProperty
      //      tester/mainwindow.cpp:    stellarSolver.setProperty("LogToFile", ui->logToFile->isChecked());
      //      tester/mainwindow.cpp:        stellarSolver.setProperty("LogFileName", filename);


      if (flagVerbosity >= 2)
        printf("INFO: Starting to solve %s\n", currentImageName);
      fflush( stdout );

      if(!stellarSolver.solve())
      {
          fprintf(stderr, "ERROR: Plate-Solve failed %s\n", currentImageName);
          continue;
      }

      FITSImage::Solution solution = stellarSolver.getSolution();
      
      FILE* f = NULL; // the output file
      
      if (!outputFilename) {
        // default: write a TOML file per image based on its name
        if (count)
          f = fopen(outputFilenameImage, flagOverwrite ? "w" : "a");
        if (f) {
          flagFileOpen=1;
          if (flagVerbosity >= 2)
            printf("INFO: Writing           %s\n",     outputFilenameImage);
        }
      } else if (strlen(outputFilename)) {
        // output file name given
        if (!strcmp(outputFilename, "stdout"))
          f = stdout;
        else if (!strcmp(outputFilename, "stderr"))
          f = stderr;
        else {
          f = fopen(outputFilename, flagOverwrite && !imageFilesCounter ? "w" : "a");
          if (f) {
            flagFileOpen=1;
            if (flagVerbosity >= 2 && !imageFilesCounter)
              printf("INFO: Writing           %s\n",     outputFilename);
          }
        }
      }
      if (!f) // when all failed, output to stdout
        f = stdout;
      
      // OUPUT FILE ------------------------------------------------------------
      time_t t;   // not a primitive datatype
      time(&t);
      fprintf(f, "# TOML entry for image %s\n", currentImageName);
      fprintf(f, "[[plate_solve]]\n");
      fprintf(f, "[solve-%s]\n", currentImageName);
      fprintf(f, "date_processed        = %s\n", ctime(&t));
      fprintf(f, "field_center          = [ %f, %f ] # deg (RA,DEC)\n", solution.ra, solution.dec);
      fprintf(f, "field_size            = [ %f, %f ] # arcminutes\n", solution.fieldWidth, solution.fieldHeight);
      fprintf(f, "field_rotation_angle  = %f # up degrees E of N\n", solution.orientation);
      fprintf(f, "field_parity          = '%s'\n", FITSImage::getParityText(solution.parity).toUtf8().data());
      fprintf(f, "pixel_scale           = %f\n", solution.pixscale);
      fflush( f );
      imageFilesCounter++;

      stellarSolver.setParameterProfile(SSolver::Parameters::ALL_STARS);

      if(!stellarSolver.extract(true))
      {
          fprintf(stderr, "ERROR: Solver Star Extraction failed %s", currentImageName);
          continue;
      }

      QList<FITSImage::Star> starList = stellarSolver.getStarList();
      fprintf(f, "stars_found           = %u\n", starList.count());
      fprintf(f, "\n");
      if (flagVerbosity >= 2)
        printf("INFO: Stars found in    %s: %u\n", currentImageName, starList.count());
        
      fprintf(f, "[[stars]]\n");
      fprintf(f, "[stars-%s]\n", currentImageName);
      for(int i=0; i < starList.count(); i++)
      {
          FITSImage::Star star = starList.at(i);
          char *ra = StellarSolver::raString(star.ra).toUtf8().data();
          char *dec = StellarSolver::decString(star.dec).toUtf8().data();
          fprintf(f, "Star #%u: (%f x, %f y), (ra: %s,dec: %s), mag: %f, peak: %f, hfr: %f \n", i, star.x, star.y, ra , dec, star.mag, star.peak, star.HFR);
      }
      if (flagFileOpen) fclose(f);
      
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
