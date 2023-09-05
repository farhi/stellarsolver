#include <QApplication>

//Includes for this project
#include "structuredefinitions.h"
#include "stellarsolver.h"
#include "ssolverutils/fileio.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
#if defined(__linux__)
    setlocale(LC_NUMERIC, "C");
#endif
    fileio imageLoader;
    imageLoader.logToSignal = false;
    if(!imageLoader.loadImage("pleiades.jpg"))
    {
        printf("Error in loading FITS file");
        exit(1);
    }
    FITSImage::Statistic stats = imageLoader.getStats();
    uint8_t *imageBuffer = imageLoader.getImageBuffer();

    StellarSolver stellarSolver(stats, imageBuffer);
    stellarSolver.setIndexFolderPaths(QStringList() << "astrometry");

    printf("Starting to solve. . .\n");
    fflush( stdout );
    if(!stellarSolver.solve())
    {
        printf("Solver Failed");
        exit(0);
    }

    FITSImage::Solution solution = stellarSolver.getSolution();
    printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

    printf("Field center: (RA,Dec) = (%f, %f) deg.\n", solution.ra, solution.dec);
    printf("Field size: %f x %f arcminutes\n", solution.fieldWidth, solution.fieldHeight);
    printf("Pixel Scale: %f\"\n", solution.pixscale);
    printf("Field rotation angle: up is %f degrees E of N\n", solution.orientation);
    printf("Field parity: %s\n\n", FITSImage::getParityText(solution.parity).toUtf8().data());
    return 0;
}
