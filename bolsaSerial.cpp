// Version serial de la Bolsa de Palabras.
// Descarga los libros de una lista de urls de Project Gutenberg, cuenta las
// palabras de cada libro y escribe la matriz en resultados/bolsa_serial.csv.
//
// Compilar:  g++ bolsaSerial.cpp -o bolsaSerial
// Correr:    .\bolsaSerial.exe urls.txt J
//            .\bolsaSerial.exe urls.txt J 10
// El segundo argumento es la letra de la computadora y el tercero las repeticiones.

#include "bolsaComun.h"
#include <ctime>


// Explica como se usa el programa cuando faltan argumentos
void mostrarUso() {
    cout << "Uso: bolsaSerial archivoDeUrls [letraDeLaComputadora] [repeticiones]\n";
    cout << "Ejemplo: bolsaSerial urls.txt J 10\n";
    cout << "El archivo debe tener una url de Project Gutenberg por renglon.\n";
}


int main(int argc, char* argv[]) {
    // El archivo de urls es obligatorio
    if (argc < 2) {
        mostrarUso();
        return 1;
    }
    string rutaUrls = argv[1];

    // La letra distingue la computadora donde se corrio, como en el primer proyecto
    string equipo = "local";
    if (argc >= 3) {
        equipo = argv[2];
    }

    // Numero de veces que se repite la medicion para sacar el promedio
    int repeticiones = 10;
    if (argc >= 4) {
        repeticiones = atoi(argv[3]);
    }
    if (repeticiones < 1) {
        repeticiones = 1;
    }

    int k = contarUrls(rutaUrls);
    if (k == 0) {
        cout << "No se encontro ninguna url en " << rutaUrls << "\n";
        mostrarUso();
        return 1;
    }

    // Arreglos con las urls, el nombre, el texto y el conteo de cada libro
    string* urls = new string[k];
    string* nombres = new string[k];
    string* libros = new string[k];
    map<string, int>* conteos = new map<string, int>[k];
    map<string, int> vocabulario;

    leerUrls(rutaUrls, urls);
    for (int i = 0; i < k; i++) {
        nombres[i] = nombreDelLibro(urls[i]);
    }

    // La descarga y la lectura de los libros no se miden
    crearCarpetas();
    if (!descargarLibros(urls, k)) {
        delete[] urls;
        delete[] nombres;
        delete[] libros;
        delete[] conteos;
        return 1;
    }
    for (int i = 0; i < k; i++) {
        libros[i] = leerLibro(rutaDelLibro(i));
    }

    // Corrida de calentamiento que no se mide, porque la primera siempre tarda mas
    contarLibros(libros, k, conteos, 0, 1);
    juntarVocabulario(conteos, k, vocabulario);

    // Se mide solo contar las palabras y juntar el vocabulario, varias veces, y se promedia.
    // Como el serial corre en un solo hilo, clock() equivale al tiempo real.
    double suma = 0;
    for (int r = 0; r < repeticiones; r++) {
        clock_t inicio = clock();
        contarLibros(libros, k, conteos, 0, 1);
        juntarVocabulario(conteos, k, vocabulario);
        clock_t fin = clock();
        suma = suma + (double) (fin - inicio) / CLOCKS_PER_SEC;
    }
    double promedio = suma / repeticiones;

    // La escritura del csv tampoco se mide
    string rutaCsv = "resultados/bolsa_serial.csv";
    escribirCsv(rutaCsv, nombres, conteos, k, vocabulario);

    cout << "\nComputadora: " << equipo << "\n";
    cout << "Libros: " << k << "\n";
    cout << "Palabras contadas: " << totalDePalabras(vocabulario) << "\n";
    cout << "Palabras distintas (columnas): " << vocabulario.size() << "\n";
    cout << "Tiempo serial promedio de " << repeticiones << " repeticiones: " << promedio << " s\n";
    cout << "Matriz escrita en " << rutaCsv << "\n";

    // Libera la memoria pedida
    delete[] urls;
    delete[] nombres;
    delete[] libros;
    delete[] conteos;
    return 0;
}
