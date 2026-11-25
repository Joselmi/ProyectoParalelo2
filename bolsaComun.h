// Funciones que comparten la version serial y la version con MPI.
// Las dos versiones cuentan con estas mismas funciones, asi cualquier
// diferencia de tiempo entre ellas viene solo del paralelismo.

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

using namespace std;


// Regresa verdadero si el caracter es un digito del 0 al 9
bool esDigito(char c) {
    if (c >= '0' && c <= '9') {
        return true;
    }
    return false;
}


// Saca el numero del libro del ultimo pedazo de la url.
// Por ejemplo de ".../ebooks/1513", de ".../pg1513.txt" y de ".../1513-0.txt"
// se obtiene "1513": se salta lo que no es digito y se toman los primeros digitos.
string numeroDelLibro(string url) {
    // Si la url termina en diagonal se le quita para no quedarnos con un pedazo vacio
    while (url.size() > 0 && url[url.size() - 1] == '/') {
        url = url.substr(0, url.size() - 1);
    }

    int ultimaDiagonal = url.rfind('/');
    string pedazo = url.substr(ultimaDiagonal + 1);

    int i = 0;
    while (i < (int) pedazo.size() && !esDigito(pedazo[i])) {
        i++;
    }

    string numero = "";
    while (i < (int) pedazo.size() && esDigito(pedazo[i])) {
        numero += pedazo[i];
        i++;
    }
    return numero;
}


// Nombre del libro que va en la primera columna del csv, por ejemplo "pg1513"
string nombreDelLibro(string url) {
    string numero = numeroDelLibro(url);
    if (numero == "") {
        return "libro";
    }
    return "pg" + numero;
}


// Convierte la url de la pagina de un libro en la url de su texto plano.
// Si la url ya termina en .txt se usa tal cual, asi tambien funcionan los espejos
// de Project Gutenberg. Si no, se arma la url del texto con el numero del libro.
string urlDelTexto(string url) {
    if (url.size() >= 4 && url.substr(url.size() - 4) == ".txt") {
        return url;
    }
    string numero = numeroDelLibro(url);
    return "https://www.gutenberg.org/cache/epub/" + numero + "/pg" + numero + ".txt";
}


// Quita espacios, tabuladores y el \r de Windows al principio y al final de un renglon
string limpiarRenglon(string renglon) {
    int inicio = 0;
    int fin = renglon.size();
    while (inicio < fin && (renglon[inicio] == ' ' || renglon[inicio] == '\t' || renglon[inicio] == '\r')) {
        inicio++;
    }
    while (fin > inicio && (renglon[fin - 1] == ' ' || renglon[fin - 1] == '\t' || renglon[fin - 1] == '\r')) {
        fin--;
    }
    return renglon.substr(inicio, fin - inicio);
}


// Cuenta cuantas urls tiene el archivo, una por renglon, sin contar renglones vacios.
// Sirve para saber de que tamano pedir los arreglos de libros.
int contarUrls(string ruta) {
    ifstream archivo(ruta);
    if (!archivo.is_open()) {
        return 0;
    }

    int total = 0;
    string renglon;
    while (getline(archivo, renglon)) {
        if (limpiarRenglon(renglon) != "") {
            total++;
        }
    }
    archivo.close();
    return total;
}


// Guarda en el arreglo urls cada url del archivo, en el mismo orden
void leerUrls(string ruta, string* urls) {
    ifstream archivo(ruta);
    int i = 0;
    string renglon;
    while (getline(archivo, renglon)) {
        string limpio = limpiarRenglon(renglon);
        if (limpio != "") {
            urls[i] = limpio;
            i++;
        }
    }
    archivo.close();
}


// Crea las carpetas donde se guardan los libros y los resultados, si no existen
void crearCarpetas() {
    filesystem::create_directory("libros");
    filesystem::create_directory("resultados");
}


// Ruta local donde se guarda el libro numero i
string rutaDelLibro(int i) {
    return "libros/libro_" + to_string(i) + ".txt";
}


// Descarga un libro con curl y lo guarda en la ruta dada.
// -f hace que curl falle si el servidor responde con error (por ejemplo 404 o 503),
// -L sigue las redirecciones y -s no imprime la barra de progreso.
// Regresa falso si la descarga fallo o si el archivo quedo vacio.
bool descargarLibro(string url, string ruta) {
    // Se borra la copia anterior para no contar un libro viejo si la descarga falla
    remove(ruta.c_str());

    string comando = "curl -f -s -L -o \"" + ruta + "\" \"" + urlDelTexto(url) + "\"";
    int resultado = system(comando.c_str());
    if (resultado != 0) {
        return false;
    }

    ifstream archivo(ruta, ios::binary);
    if (!archivo.is_open()) {
        return false;
    }
    archivo.seekg(0, ios::end);
    long long tamano = archivo.tellg();
    archivo.close();
    if (tamano <= 0) {
        return false;
    }
    return true;
}


// Descarga todos los libros de la lista. Si alguno falla avisa cual fue y regresa falso.
bool descargarLibros(string* urls, int k) {
    for (int i = 0; i < k; i++) {
        cout << "Descargando " << urlDelTexto(urls[i]) << "\n";
        if (!descargarLibro(urls[i], rutaDelLibro(i))) {
            cout << "No se pudo descargar " << urlDelTexto(urls[i]) << "\n";
            return false;
        }
    }
    return true;
}


// Lee todo el archivo de un libro y lo regresa en un solo string
string leerLibro(string ruta) {
    ifstream archivo(ruta, ios::binary);
    stringstream contenido;
    contenido << archivo.rdbuf();
    archivo.close();
    return contenido.str();
}


// Regresa verdadero si el caracter forma parte de una palabra: letras ASCII y digitos.
// Todo lo demas (espacios, puntuacion, guion bajo, apostrofos y bytes que no son ASCII,
// como los acentos o las comillas curvas) separa palabras.
bool esParteDePalabra(char c) {
    if (c >= 'a' && c <= 'z') {
        return true;
    }
    if (c >= 'A' && c <= 'Z') {
        return true;
    }
    if (esDigito(c)) {
        return true;
    }
    return false;
}


// Pasa una letra mayuscula a minuscula, los demas caracteres los deja igual
char aMinuscula(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c - 'A' + 'a';
    }
    return c;
}


// Suma una palabra al diccionario, pero solo si tiene 2 o mas caracteres,
// igual que CountVectorizer en la libreta de la clase
void agregarPalabra(string palabra, map<string, int>& conteo) {
    if (palabra.size() >= 2) {
        conteo[palabra]++;
    }
}


// Cuenta las palabras del pedazo del texto que va de la posicion inicio
// hasta antes de la posicion fin. Recorre caracter por caracter: mientras sea
// parte de una palabra la va armando en minusculas, y cuando encuentra un
// separador suma la palabra que llevaba al diccionario y empieza otra.
void contarPalabras(string& texto, long long inicio, long long fin, map<string, int>& conteo) {
    string palabra = "";
    for (long long i = inicio; i < fin; i++) {
        char c = texto[i];
        if (esParteDePalabra(c)) {
            palabra += aMinuscula(c);
        } else {
            agregarPalabra(palabra, conteo);
            palabra = "";
        }
    }
    // La ultima palabra del pedazo no tiene separador despues, se suma aqui
    agregarPalabra(palabra, conteo);
}


// Calcula donde empieza la rebanada del texto que le toca a un proceso.
// La rebanada sin ajustar empieza en tamano * rank / procesos. Una palabra le
// pertenece al proceso en cuya rebanada cae su primer caracter, entonces si el
// inicio cae a la mitad de una palabra, esa palabra es del proceso anterior y
// el inicio se recorre hasta que la palabra termina.
long long inicioDeRebanada(string& texto, int rank, int procesos) {
    long long tamano = texto.size();
    long long inicio = tamano * rank / procesos;
    if (inicio > 0) {
        while (inicio < tamano && esParteDePalabra(texto[inicio]) && esParteDePalabra(texto[inicio - 1])) {
            inicio++;
        }
    }
    return inicio;
}


// La rebanada de un proceso termina justo donde empieza la del siguiente.
// Como los dos usan la misma regla, ninguna palabra se cuenta dos veces ni se pierde.
// Para el ultimo proceso el inicio del "siguiente" es el final del texto.
long long finDeRebanada(string& texto, int rank, int procesos) {
    return inicioDeRebanada(texto, rank + 1, procesos);
}


// Cuenta la rebanada que le toca a este proceso de cada uno de los k libros.
// Cada libro tiene su propio diccionario. Antes de contar se vacian los
// diccionarios para que cada repeticion empiece de cero.
// La version serial lo llama con rank 0 y 1 proceso, asi cuenta los libros completos.
void contarLibros(string* libros, int k, map<string, int>* conteos, int rank, int procesos) {
    for (int i = 0; i < k; i++) {
        conteos[i].clear();
        long long inicio = inicioDeRebanada(libros[i], rank, procesos);
        long long fin = finDeRebanada(libros[i], rank, procesos);
        contarPalabras(libros[i], inicio, fin, conteos[i]);
    }
}


// Junta en un solo diccionario todas las palabras que aparecen en algun libro.
// El valor es cuantas veces aparece la palabra sumando todos los libros.
// Como es un map, las palabras quedan en orden alfabetico, que es el orden
// de las columnas del csv.
void juntarVocabulario(map<string, int>* conteos, int k, map<string, int>& vocabulario) {
    vocabulario.clear();
    for (int i = 0; i < k; i++) {
        for (map<string, int>::iterator it = conteos[i].begin(); it != conteos[i].end(); ++it) {
            vocabulario[it->first] += it->second;
        }
    }
}


// Suma cuantas palabras se contaron en total en todos los libros
long long totalDePalabras(map<string, int>& vocabulario) {
    long long total = 0;
    for (map<string, int>::iterator it = vocabulario.begin(); it != vocabulario.end(); ++it) {
        total += it->second;
    }
    return total;
}


// Escribe la matriz de Bolsa de Palabras en csv.
// El primer renglon es "libro" seguido de todas las palabras del vocabulario,
// y despues va un renglon por libro con su nombre y cuantas veces aparece
// cada palabra en ese libro, con 0 si no aparece.
void escribirCsv(string ruta, string* nombres, map<string, int>* conteos, int k, map<string, int>& vocabulario) {
    // Se abre en modo binario para que en Windows los renglones terminen en \n y no en \r\n,
    // asi el csv sale exactamente igual en Windows y en Linux
    ofstream archivo(ruta, ios::binary);

    archivo << "libro";
    for (map<string, int>::iterator it = vocabulario.begin(); it != vocabulario.end(); ++it) {
        archivo << "," << it->first;
    }
    archivo << "\n";

    for (int i = 0; i < k; i++) {
        archivo << nombres[i];
        for (map<string, int>::iterator it = vocabulario.begin(); it != vocabulario.end(); ++it) {
            if (conteos[i].count(it->first) > 0) {
                archivo << "," << conteos[i].at(it->first);
            } else {
                archivo << ",0";
            }
        }
        archivo << "\n";
    }
    archivo.close();
}
