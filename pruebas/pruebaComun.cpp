// Pruebas de las funciones de bolsaComun.h.
// Cada prueba imprime OK o FALLA, y al final cuantas fallaron.
// Se compila desde la carpeta del proyecto con:
//   g++ pruebas/pruebaComun.cpp -o pruebas/pruebaComun

#include "../bolsaComun.h"

int fallas = 0;


// Imprime el resultado de una prueba y lleva la cuenta de las que fallan
void revisar(string nombre, bool correcto) {
    if (correcto) {
        cout << "OK     " << nombre << "\n";
    } else {
        cout << "FALLA  " << nombre << "\n";
        fallas++;
    }
}


// Pruebas de las urls: pagina del libro, texto oficial y espejo
void probarUrls() {
    revisar("url de pagina a url de texto",
            urlDelTexto("https://www.gutenberg.org/ebooks/1513") == "https://www.gutenberg.org/cache/epub/1513/pg1513.txt");
    revisar("url de pagina con diagonal al final",
            urlDelTexto("https://www.gutenberg.org/ebooks/1513/") == "https://www.gutenberg.org/cache/epub/1513/pg1513.txt");
    revisar("url .txt se usa tal cual",
            urlDelTexto("http://mirrors.xmission.com/gutenberg/1/5/1/1513/1513-0.txt") == "http://mirrors.xmission.com/gutenberg/1/5/1/1513/1513-0.txt");
    revisar("nombre desde pagina", nombreDelLibro("https://www.gutenberg.org/ebooks/1513") == "pg1513");
    revisar("nombre desde texto oficial", nombreDelLibro("https://www.gutenberg.org/cache/epub/1513/pg1513.txt") == "pg1513");
    revisar("nombre desde espejo", nombreDelLibro("http://mirrors.xmission.com/gutenberg/1/5/1/1513/1513-0.txt") == "pg1513");
}


// Pruebas de los escenarios de la regla para separar palabras
void probarConteo() {
    map<string, int> conteo;

    string texto1 = "Hamlet, HAMLET! hamlet.";
    contarPalabras(texto1, 0, texto1.size(), conteo);
    revisar("mayusculas y puntuacion: hamlet = 3", conteo["hamlet"] == 3 && conteo.size() == 1);

    conteo.clear();
    string texto2 = "I am a man";
    contarPalabras(texto2, 0, texto2.size(), conteo);
    revisar("palabras de un caracter no cuentan",
            conteo.size() == 2 && conteo.count("am") == 1 && conteo.count("man") == 1);

    conteo.clear();
    string texto3 = "don't";
    contarPalabras(texto3, 0, texto3.size(), conteo);
    revisar("apostrofo: solo don", conteo.size() == 1 && conteo["don"] == 1);

    conteo.clear();
    string texto4 = "_hamlet_ caf\xc3\xa9 r2d2";
    contarPalabras(texto4, 0, texto4.size(), conteo);
    revisar("guion bajo y bytes no ASCII separan",
            conteo.size() == 3 && conteo["hamlet"] == 1 && conteo["caf"] == 1 && conteo["r2d2"] == 1);
}


// Prueba que repartir el texto en rebanadas da el mismo conteo que el texto completo,
// para todos los numeros de procesos de 1 a 8 y con un texto donde las fronteras
// caen a la mitad de muchas palabras
void probarRebanadas() {
    string texto = "To be, or not to be, that is the question: Whether 'tis nobler in the mind "
                   "to suffer the slings and arrows of outrageous fortune, or to take arms "
                   "against a sea of troubles. Supercalifragilistic word123 x";
    map<string, int> completo;
    contarPalabras(texto, 0, texto.size(), completo);

    for (int procesos = 1; procesos <= 8; procesos++) {
        map<string, int> suma;
        long long anterior = 0;
        bool continuas = true;
        for (int rank = 0; rank < procesos; rank++) {
            long long inicio = inicioDeRebanada(texto, rank, procesos);
            long long fin = finDeRebanada(texto, rank, procesos);
            // Cada rebanada debe empezar donde termino la anterior
            if (inicio != anterior) {
                continuas = false;
            }
            anterior = fin;

            map<string, int> parte;
            contarPalabras(texto, inicio, fin, parte);
            for (map<string, int>::iterator it = parte.begin(); it != parte.end(); ++it) {
                suma[it->first] += it->second;
            }
        }
        bool cubreTodo = (anterior == (long long) texto.size());
        revisar("rebanadas con " + to_string(procesos) + " procesos", suma == completo && continuas && cubreTodo);
    }

    // Mas procesos que caracteres: algunos procesos se quedan con rebanadas vacias
    string corto = "hola mundo";
    map<string, int> completoCorto;
    contarPalabras(corto, 0, corto.size(), completoCorto);
    map<string, int> sumaCorto;
    for (int rank = 0; rank < 16; rank++) {
        contarPalabras(corto, inicioDeRebanada(corto, rank, 16), finDeRebanada(corto, rank, 16), sumaCorto);
    }
    revisar("texto corto con 16 procesos", sumaCorto == completoCorto);
}


// Prueba el vocabulario y el csv con dos libros pequenos
void probarCsv() {
    int k = 2;
    string* libros = new string[k];
    libros[0] = "love love zounds";
    libros[1] = "love youth";
    string* nombres = new string[k];
    nombres[0] = "pg1";
    nombres[1] = "pg2";
    map<string, int>* conteos = new map<string, int>[k];
    map<string, int> vocabulario;

    contarLibros(libros, k, conteos, 0, 1);
    juntarVocabulario(conteos, k, vocabulario);
    revisar("vocabulario con 3 palabras", vocabulario.size() == 3 && vocabulario["love"] == 3);
    revisar("total de palabras", totalDePalabras(vocabulario) == 5);

    escribirCsv("pruebas/prueba.csv", nombres, conteos, k, vocabulario);
    string esperado = "libro,love,youth,zounds\npg1,2,0,1\npg2,1,1,0\n";
    revisar("csv con ceros donde no aparece la palabra", leerLibro("pruebas/prueba.csv") == esperado);
    remove("pruebas/prueba.csv");

    delete[] libros;
    delete[] nombres;
    delete[] conteos;
}


int main() {
    probarUrls();
    probarConteo();
    probarRebanadas();
    probarCsv();
    cout << "\nPruebas que fallaron: " << fallas << "\n";
    return fallas;
}
