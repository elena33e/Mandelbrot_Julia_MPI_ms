#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <mpi.h>

// Dimensiunea ferestrei
const int WIDTH = 1024;
const int HEIGHT = 768;
const int MAX_ITER = 1000;

// Tags pentru control flux
enum { TAG_TASK, TAG_RESULT, TAG_STOP };

struct Config {
    double xStart, yStart, width, height;
    bool isJulia;
    double cRe, cIm;
};

struct PixelRGB {
    unsigned char r, g, b;
};

// Metoda calcul fractal
inline int computeFractal(double zr, double zi, double cr, double ci) {
    int i = 0;
    while (zr * zr + zi * zi <= 4.0 && i < MAX_ITER) {
        double temp_zr = zr * zr - zi * zi + cr;
        zi = 2.0 * zr * zi + ci;
        zr = temp_zr;
        i++;
    }
    return i;
}

// Salvare imagine PPM 
void saveToPPM(const std::string& filename, const std::vector<int>& allData) {
    std::ofstream img(filename, std::ios::binary);
    if (!img.is_open()) return;
    img << "P6\n" << WIDTH << " " << HEIGHT << "\n255\n";
    for (int i = 0; i < WIDTH * HEIGHT; ++i) {
        int iter = allData[i];
        PixelRGB pixel = { 0, 0, 0 };
        if (iter != MAX_ITER) {
            pixel.r = (iter * 9) % 256;
            pixel.g = (iter * 2) % 256;
            pixel.b = (iter * 15) % 256;
        }
        img.write(reinterpret_cast<const char*>(&pixel), sizeof(PixelRGB));
    }
    img.close();
}

void runDynamicJob(Config& conf, int rank, int size, const std::string& filename) {
    // Sincronizarea configuratiei
    MPI_Bcast(&conf, sizeof(Config), MPI_BYTE, 0, MPI_COMM_WORLD);

    double startTime = MPI_Wtime();

    if (rank == 0) {
        // Logica pe master
        std::vector<int> finalImage(WIDTH * HEIGHT);
        int nextRow = 0;
        int workersActive = 0;
        long totalItersGlobal = 0;

        // Distribuim primele sarcini catre toti workerii
        for (int i = 1; i < size; i++) {
            if (nextRow < HEIGHT) {
                MPI_Send(&nextRow, 1, MPI_INT, i, TAG_TASK, MPI_COMM_WORLD);
                nextRow++;
                workersActive++;
            }
            else {
                int dummy = -1;
                MPI_Send(&dummy, 1, MPI_INT, i, TAG_STOP, MPI_COMM_WORLD);
            }
        }

        // Buffer pentru primirea rezultatelor: [0] = indexul randului, [1..WIDTH] = datele
        std::vector<int> recvBuffer(WIDTH + 1);

        while (workersActive > 0) {
            MPI_Status status;
            MPI_Recv(recvBuffer.data(), WIDTH + 1, MPI_INT, MPI_ANY_SOURCE, TAG_RESULT, MPI_COMM_WORLD, &status);

            int workerId = status.MPI_SOURCE;
            int completedRow = recvBuffer[0];

            // Asamblam randul in imaginea finala
            for (int x = 0; x < WIDTH; x++) {
                finalImage[completedRow * WIDTH + x] = recvBuffer[x + 1];
                totalItersGlobal += recvBuffer[x + 1];
            }

            // Daca mai sunt randuri, trimitem imediat unul nou workerului care s-a eliberat
            if (nextRow < HEIGHT) {
                MPI_Send(&nextRow, 1, MPI_INT, workerId, TAG_TASK, MPI_COMM_WORLD);
                nextRow++;
            }
            else {
                // Oprire worker
                int stopSignal = -1;
                MPI_Send(&stopSignal, 1, MPI_INT, workerId, TAG_STOP, MPI_COMM_WORLD);
                workersActive--;
            }
        }

        double endTime = MPI_Wtime();
        printf("[%s] Finalizat in %.4f sec. Total iteratii: %ld\n", filename.c_str(), endTime - startTime, totalItersGlobal);
        saveToPPM(filename, finalImage);

    }
    else {
        // Logica pe worker
        std::vector<int> sendBuffer(WIDTH + 1); // [0] = index rand, [1..WIDTH] = rezultate

        while (true) {
            int rowToProcess;
            MPI_Status status;

            // Primeste sarcina de la master
            MPI_Recv(&rowToProcess, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

            if (status.MPI_TAG == TAG_STOP) break;

            sendBuffer[0] = rowToProcess;
            double py = conf.yStart + (double)rowToProcess * conf.height / HEIGHT;

            for (int x = 0; x < WIDTH; x++) {
                double px = conf.xStart + (double)x * conf.width / WIDTH;
                int iter;
                if (conf.isJulia)
                    iter = computeFractal(px, py, conf.cRe, conf.cIm);
                else
                    iter = computeFractal(0.0, 0.0, px, py);

                sendBuffer[x + 1] = iter;
            }

            // Trimitem randul calculat inapoi la Master
            MPI_Send(sendBuffer.data(), WIDTH + 1, MPI_INT, 0, TAG_RESULT, MPI_COMM_WORLD);
        }
    }
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size < 2) {
        if (rank == 0) std::cerr << "DLB necesita minim 2 procese (1 Master, 1 Worker).\n";
        MPI_Finalize(); return 1;
    }

    //1. Fractlul Mandelbrot
    Config mandel;
    if (rank == 0) mandel = { -2.0, -1.2, 3.0, 2.4, false, 0, 0 };
    runDynamicJob(mandel, rank, size, "mandelbrot_dlb.ppm");

    // 2. Fractali Julia
    std::vector<std::pair<double, double>> seeds = {
        {-0.8, 0.156},
        {-0.7269, 0.1889},
        {0.285, 0.01}
    };

    for (int i = 0; i < (int)seeds.size(); ++i) {
        Config julia;
        if (rank == 0) julia = { -1.5, -1.5, 3.0, 3.0, true, seeds[i].first, seeds[i].second };
        runDynamicJob(julia, rank, size, "julia_dlb_" + std::to_string(i) + ".ppm");
    }

    MPI_Finalize();
    return 0;
}