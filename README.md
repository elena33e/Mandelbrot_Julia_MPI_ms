
# Fractal Generator - MPI Dynamic Load Balancing

Această aplicație realizează generarea în paralel a fractalilor din mulțimile **Mandelbrot** și **Julia**, utilizând limbajul **C++** și interfața **MPI (Message Passing Interface)**. Proiectul se concentrează pe utilizarea unei strategii de echilibrare dinamică a sarcinii (**Dynamic Load Balancing**) pentru a maximiza eficiența procesării pe sisteme multi-core.

## Caracteristici
* **Model de calcul:** Arhitectură Master-Worker.
* **Echilibrare Dinamică (DLB):** Distribuție rând-cu-rând, asigurând utilizarea constantă a tuturor proceselor Worker, indiferent de complexitatea computațională a diferitelor regiuni ale fractalului.
* **Fractali generați:** Mulțimea Mandelbrot și 3 variante de seturi Julia (folosind seed-uri diferite).
* **Format Ieșire:** Imagini brute în format `.ppm` (Portable Pixmap).

## Logica de funcționare

Programul utilizează trei etichete (**Tags**) pentru a controla fluxul de date:
1.  **TAG_TASK:** Trimis de Master către Worker pentru a indica indicele rândului ce trebuie procesat.
2.  **TAG_RESULT:** Trimis de Worker către Master împreună cu valorile calculate pentru rândul respectiv.
3.  **TAG_STOP:** Trimis de Master către Worker pentru a semnaliza finalizarea execuției.

### Roluri:
* **Master (Rank 0):** Gestionează coada de sarcini, distribuie rândurile către procesele libere, asamblează imaginea finală și salvează rezultatele pe disc.
* **Worker (Rank > 0):** Primește indicele rândului, calculează valoarea fractalului pentru fiecare pixel și trimite setul de date înapoi la Master.

## Cum se rulează

### Cerințe
* Un compilator C++ (GCC/MSVC).
* O implementare MPI instalată (Microsoft MPI, OpenMPI sau MPICH).

### Compilare
Dacă folosești terminalul (ex: Linux sau MS-MPI Command Prompt):
```bash
mpicxx -o fractal_generator main.cpp

### Execuție
Pentru a rula programul cu 8 procese (1 Master + 7 Workers):

Bash
mpiexec -n 8 ./fractal_generator

### Rezultate
La finalul execuției, programul generează automat următoarele fișiere:

mandelbrot_dlb.ppm

julia_dlb_0.ppm

julia_dlb_1.ppm

julia_dlb_2.ppm

În consolă va fi afișat timpul de execuție pentru fiecare sarcină și numărul total de iterații calculate.
