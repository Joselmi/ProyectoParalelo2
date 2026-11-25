// Version paralela con MPI de la Bolsa de Palabras.
// Cada proceso cuenta una rebanada de cada libro y manda sus conteos al
// proceso maestro, que los suma, junta el vocabulario y escribe la matriz en
// resultados/bolsa_mpi.csv. El maestro tambien mide la version serial con las
// mismas funciones para calcular el speedup en la misma corrida.
//
// Compilar en Windows con MS-MPI:
//   g++ bolsaMPI.cpp -o bolsaMPI -I "C:\msmpi\Sdk\Include" -L "C:\msmpi\Sdk\Lib\x64" -lmsmpi
// Correr con q procesos:
//   mpiexec -n 4 .\bolsaMPI.exe urls.txt J
//   mpiexec -n 4 .\bolsaMPI.exe urls.txt J 10
// El segundo argumento es la letra de la computadora y el tercero las repeticiones.

#include <mpi.h>
#include "bolsaComun.h"

// El proceso 0 es el maestro, igual que en mpi_hello_world.cpp de la clase
const int MASTER = 0;

// Etiquetas para distinguir los dos mensajes que manda cada proceso al maestro
const int ETIQUETA_TAMANO = 0;
const int ETIQUETA_TEXTO = 1;


// Explica como se usa el programa cuando faltan argumentos
void mostrarUso() {
    cout << "Uso: mpiexec -n procesos bolsaMPI archivoDeUrls [letraDeLaComputadora] [repeticiones]\n";
    cout << "Ejemplo: mpiexec -n 4 bolsaMPI urls.txt J 10\n";
    cout << "El archivo debe tener una url de Project Gutenberg por renglon.\n";
}


// Convierte los conteos de este proceso en un solo texto para poder mandarlo
// por MPI. Cada renglon es "libro palabra cantidad", por ejemplo "0 love 10".
// Los diccionarios no se pueden mandar directo porque MPI solo manda arreglos
// de datos simples, como numeros o caracteres.
string conteosATexto(map<string, int>* conteos, int k) {
    ostringstream texto;
    for (int i = 0; i < k; i++) {
        for (map<string, int>::iterator it = conteos[i].begin(); it != conteos[i].end(); ++it) {
            texto << i << " " << it->first << " " << it->second << "\n";
        }
    }
    return texto.str();
}


// Lee el texto que mando otro proceso y suma cada conteo al diccionario del
// libro que le corresponde en el maestro
void sumarTexto(string& texto, map<string, int>* conteos) {
    istringstream entrada(texto);
    int libro;
    string palabra;
    int cantidad;
    while (entrada >> libro >> palabra >> cantidad) {
        conteos[libro][palabra] += cantidad;
    }
}


// Manda los conteos de este proceso al maestro con dos mensajes punto a punto:
// primero el tamano del texto, para que el maestro sepa cuanta memoria pedir,
// y despues el texto mismo
void mandarConteos(map<string, int>* conteos, int k) {
    string texto = conteosATexto(conteos, k);
    int tamano = texto.size();
    MPI_Send(&tamano, 1, MPI_INT, MASTER, ETIQUETA_TAMANO, MPI_COMM_WORLD);
    MPI_Send(texto.c_str(), tamano, MPI_CHAR, MASTER, ETIQUETA_TEXTO, MPI_COMM_WORLD);
}


// El maestro recibe los conteos de cada uno de los demas procesos, en orden,
// y los suma a sus propios conteos. Se usa MPI_Send y MPI_Recv en lugar de
// MPI_Reduce porque cada proceso tiene un vocabulario distinto y de distinto
// tamano, y MPI_Reduce solo suma arreglos de numeros del mismo tamano.
void recibirConteos(map<string, int>* conteos, int numProcesos) {
    for (int origen = 1; origen < numProcesos; origen++) {
        int tamano = 0;
        MPI_Recv(&tamano, 1, MPI_INT, origen, ETIQUETA_TAMANO, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        char* buffer = new char[tamano];
        MPI_Recv(buffer, tamano, MPI_CHAR, origen, ETIQUETA_TEXTO, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        string texto(buffer, tamano);
        delete[] buffer;

        sumarTexto(texto, conteos);
    }
}


// Una corrida completa de la version paralela: cada proceso cuenta su rebanada
// de cada libro, los demas mandan sus conteos y el maestro los suma y junta
// el vocabulario. Al terminar, solo los conteos del maestro tienen el total.
void bolsaParalela(string* libros, int k, map<string, int>* conteos, map<string, int>& vocabulario,
                   int idProceso, int numProcesos) {
    contarLibros(libros, k, conteos, idProceso, numProcesos);
    if (idProceso != MASTER) {
        mandarConteos(conteos, k);
    } else {
        recibirConteos(conteos, numProcesos);
        juntarVocabulario(conteos, k, vocabulario);
    }
}


// Una corrida completa de la version serial: cuenta los libros completos y
// junta el vocabulario. Es lo mismo que hace bolsaSerial.cpp.
void bolsaSerial(string* libros, int k, map<string, int>* conteos, map<string, int>& vocabulario) {
    contarLibros(libros, k, conteos, 0, 1);
    juntarVocabulario(conteos, k, vocabulario);
}


// Cuenta cuantas celdas de la matriz son diferentes entre dos conteos.
// Revisa las palabras del primero contra el segundo, y luego las palabras
// que solo estan en el segundo.
int contarDiferencias(map<string, int>* a, map<string, int>* b, int k) {
    int diferencias = 0;
    for (int i = 0; i < k; i++) {
        for (map<string, int>::iterator it = a[i].begin(); it != a[i].end(); ++it) {
            if (b[i].count(it->first) == 0 || b[i].at(it->first) != it->second) {
                diferencias++;
            }
        }
        for (map<string, int>::iterator it = b[i].begin(); it != b[i].end(); ++it) {
            if (a[i].count(it->first) == 0) {
                diferencias++;
            }
        }
    }
    return diferencias;
}


// Agrega un renglon con los tiempos de esta corrida al archivo de tiempos.
// Si el archivo no existe todavia, primero escribe el encabezado.
void guardarTiempos(string ruta, string rutaUrls, int numProcesos, int k, double serial, double paralelo, double speedup) {
    bool existe = false;
    ifstream prueba(ruta);
    if (prueba.is_open()) {
        existe = true;
        prueba.close();
    }

    ofstream archivo(ruta, ios::app | ios::binary);
    if (!existe) {
        archivo << "urls,procesos,libros,serial,paralelo,speedup\n";
    }
    archivo << rutaUrls << "," << numProcesos << "," << k << "," << serial << "," << paralelo << "," << speedup << "\n";
    archivo.close();
}


int main(int argc, char* argv[]) {
    int numProcesos = 0;
    int idProceso = 0;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numProcesos);
    MPI_Comm_rank(MPI_COMM_WORLD, &idProceso);

    // El archivo de urls es obligatorio. Todos los procesos revisan los argumentos
    // y terminan juntos, pero solo el maestro imprime el mensaje.
    if (argc < 2) {
        if (idProceso == MASTER) {
            mostrarUso();
        }
        MPI_Finalize();
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

    // Cada proceso lee la lista de urls por su cuenta, porque esta en el disco
    int k = contarUrls(rutaUrls);
    if (k == 0) {
        if (idProceso == MASTER) {
            cout << "No se encontro ninguna url en " << rutaUrls << "\n";
            mostrarUso();
        }
        MPI_Finalize();
        return 1;
    }

    // Arreglos con las urls, el nombre, el texto y el conteo de cada libro.
    // El maestro tiene ademas los conteos de la version serial para comparar.
    string* urls = new string[k];
    string* nombres = new string[k];
    string* libros = new string[k];
    map<string, int>* conteos = new map<string, int>[k];
    map<string, int>* conteosSerial = new map<string, int>[k];
    map<string, int> vocabulario;
    map<string, int> vocabularioSerial;

    leerUrls(rutaUrls, urls);
    for (int i = 0; i < k; i++) {
        nombres[i] = nombreDelLibro(urls[i]);
    }

    // Solo el maestro descarga los libros. Despues le avisa a todos con MPI_Bcast
    // si la descarga salio bien; los demas procesos se quedan esperando ese aviso,
    // asi nadie lee un libro antes de que termine de descargarse.
    int descargaCorrecta = 0;
    if (idProceso == MASTER) {
        crearCarpetas();
        if (descargarLibros(urls, k)) {
            descargaCorrecta = 1;
        }
    }
    MPI_Bcast(&descargaCorrecta, 1, MPI_INT, MASTER, MPI_COMM_WORLD);
    if (descargaCorrecta == 0) {
        delete[] urls;
        delete[] nombres;
        delete[] libros;
        delete[] conteos;
        delete[] conteosSerial;
        MPI_Finalize();
        return 1;
    }

    // Cada proceso lee los libros del disco a su propia memoria. No se mide.
    for (int i = 0; i < k; i++) {
        libros[i] = leerLibro(rutaDelLibro(i));
    }

    // Version serial, solo en el maestro, mientras los demas esperan en la barrera.
    // Se mide con MPI_Wtime, el mismo reloj que la version paralela.
    double promedioSerial = 0;
    if (idProceso == MASTER) {
        // Corrida de calentamiento que no se mide, porque la primera siempre tarda mas
        bolsaSerial(libros, k, conteosSerial, vocabularioSerial);
        double suma = 0;
        for (int r = 0; r < repeticiones; r++) {
            double inicio = MPI_Wtime();
            bolsaSerial(libros, k, conteosSerial, vocabularioSerial);
            double fin = MPI_Wtime();
            suma = suma + (fin - inicio);
        }
        promedioSerial = suma / repeticiones;
    }
    MPI_Barrier(MPI_COMM_WORLD);

    // Corrida de calentamiento de la version paralela, que no se mide.
    // Tambien sirve para que cada proceso diga cuanto texto le toco.
    bolsaParalela(libros, k, conteos, vocabulario, idProceso, numProcesos);
    long long palabrasPropias = 0;
    if (idProceso != MASTER) {
        // En los demas procesos los conteos son solo de su rebanada
        map<string, int> propias;
        juntarVocabulario(conteos, k, propias);
        palabrasPropias = totalDePalabras(propias);
        cout << "Proceso " << idProceso << " de " << numProcesos << " leyo " << k << " libros y conto "
             << palabrasPropias << " palabras de sus rebanadas\n";
    }

    // Version paralela: todos empiezan juntos despues de la barrera, y el maestro
    // detiene el reloj cuando termino de sumar los conteos de todos y de juntar
    // el vocabulario. Ese es el tiempo de una repeticion.
    double sumaParalela = 0;
    for (int r = 0; r < repeticiones; r++) {
        MPI_Barrier(MPI_COMM_WORLD);
        double inicio = MPI_Wtime();
        bolsaParalela(libros, k, conteos, vocabulario, idProceso, numProcesos);
        double fin = MPI_Wtime();
        sumaParalela = sumaParalela + (fin - inicio);
    }

    if (idProceso == MASTER) {
        double promedioParalelo = sumaParalela / repeticiones;
        double speedup = promedioSerial / promedioParalelo;

        // Se comprueba que la matriz paralela sea identica a la serial
        int diferencias = contarDiferencias(conteos, conteosSerial, k);

        // La escritura del csv y de los tiempos no se mide
        string rutaCsv = "resultados/bolsa_mpi.csv";
        escribirCsv(rutaCsv, nombres, conteos, k, vocabulario);
        string rutaTiempos = "resultados/tiempos_" + equipo + ".csv";
        guardarTiempos(rutaTiempos, rutaUrls, numProcesos, k, promedioSerial, promedioParalelo, speedup);

        cout << "\nComputadora: " << equipo << "\n";
        cout << "Libros: " << k << "\n";
        cout << "Palabras contadas: " << totalDePalabras(vocabulario) << "\n";
        cout << "Palabras distintas (columnas): " << vocabulario.size() << "\n";
        if (diferencias == 0) {
            cout << "La matriz paralela es identica a la serial\n";
        } else {
            cout << "MISMATCH: la matriz paralela tiene " << diferencias << " celdas diferentes a la serial\n";
        }
        cout << "Procesos: " << numProcesos << "\n";
        cout << "Tiempo serial promedio:   " << promedioSerial << " s\n";
        cout << "Tiempo paralelo promedio: " << promedioParalelo << " s\n";
        cout << "Speedup: " << speedup << "x\n";
        cout << "Matriz escrita en " << rutaCsv << " y tiempos en " << rutaTiempos << "\n";
    }

    // Libera la memoria pedida
    delete[] urls;
    delete[] nombres;
    delete[] libros;
    delete[] conteos;
    delete[] conteosSerial;

    MPI_Finalize();
    return 0;
}
