#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>


//Declaración variables globales
FILE *logFile;
int TRUE = 1;
pthread_mutex_t semaforoReponedor;
pthread_mutex_t semaforoAtencionCliente;
pthread_mutex_t semaforoCola;
pthread_mutex_t semaforoFichero;
pthread_mutex_t semaforoColaAtencionCliente;
pthread_cond_t condicionReponedor;
pthread_cond_t condicionAtencionCliente;
pthread_t hiloReponedor;
pthread_t hiloAtencionCliente;
int contadorClientes;
int contadorClientesAtencionCliente;
int numeroCajeros;
int numeroClientes;
struct cliente{
	int id; //Empieza en 1
	int estado; //0 = no atendido; 1 = está siendo atendido; 2 = ha sido atendido; 3 = va a atención al cliente; 4 = es atendido por atención al cliente
	int posicionAtencionCliente; //Para ver en que puesto llega a atención al cliente
	pthread_t hiloCliente;
};
struct cliente *clientes;
struct cliente *atencionCliente;
struct cajero{
	int id; //Empieza en 1
	int contadorClientes;
	int descansado;
	pthread_t hiloCajero;
};
struct cajero *cajeros;
char *logFileName;


void writeLogMessage(int idCiente, int idCajero, char *msgPrimeraParte, char *msgSegundaParte, int precio);

int calculaAleatorio (int min, int max, int id);

void manejadorSIGUSR1();

void manejadorSIGUSR2();

void nuevoCliente();

int haySitioEnCola();

int haySitioEnAtencionCliente();

void *accionesCliente(void* sitioEnCola);

void liberarSitioEnCola(int index);

void liberarSitioEnAtencionCliente(int index);

void nuevoCajero(int id);

void *accionesCajero(void* id);

int siguienteCliente();

void atenderBien(int idCliente, int idCajeros);

void *accionesReponedor(void * nada);

void *accionesAtencionCliente(void * nada);

int siguienteClienteAtencionCliente();

void acabarPrograma();




/*Ejecución del programa, el primer parámetro es
*el número de cajeros y el segundo el tamaño de la cola.
*/
int main(int argc, char  *argv[]){
	//Definición de manejadores de las señales
	if(signal(SIGUSR1, manejadorSIGUSR1) == SIG_ERR){//Entrada de clientes
		perror("llamada a SIGUSR1.\n");
		exit(-1);
	}
	
	if(signal(SIGUSR2, manejadorSIGUSR2) == SIG_ERR){//Añadir un cajero y que quepa un cliente en la cola más en ejecución
		perror("llamada a SIGUSR2.\n");
		exit(-1);
	}
	
	 if(signal(SIGINT, acabarPrograma) == SIG_ERR){
		perror("llamada a SIGINT.\n");
		exit(-1);
	}
	
	//Declaración de variables locales
	int x;
	int i;
	pthread_mutex_t semaforoInicializarVariables;
	pthread_mutex_init(&semaforoInicializarVariables, NULL);
	
	pthread_mutex_lock(&semaforoInicializarVariables);
	//Sección crítica inicialización de semáforos y variables
	pthread_mutex_init(&semaforoReponedor, NULL);	
	pthread_mutex_init(&semaforoAtencionCliente, NULL);
	pthread_mutex_init(&semaforoColaAtencionCliente, NULL);
	pthread_mutex_init(&semaforoFichero, NULL);
	pthread_mutex_init(&semaforoCola, NULL);
	pthread_cond_init(&condicionReponedor, NULL);
	pthread_cond_init(&condicionAtencionCliente, NULL);
	numeroCajeros = atoi(argv[1]);
	numeroClientes = atoi(argv[2]);
	cajeros = (struct cajero*) malloc(sizeof(struct cajero) * numeroCajeros);
	clientes = (struct cliente*) malloc(sizeof(struct cliente) * numeroClientes);
	atencionCliente = (struct cliente*) malloc(sizeof(struct cliente) * numeroClientes);
	pthread_create(&hiloReponedor, NULL, accionesReponedor, NULL); //Creación del reponedor
	pthread_create(&hiloAtencionCliente, NULL, accionesAtencionCliente, NULL); //Creación del reponedor
	//Fin sección crítica inicialización de variables
	pthread_mutex_unlock(&semaforoInicializarVariables);
	
	pthread_mutex_lock(&semaforoFichero);
	logFileName = "registroCaja.log";
	logFile = fopen(logFileName, "w");
	fclose(logFile);
	pthread_mutex_unlock(&semaforoFichero);
	
	
	//Inicializo la cola de clientes
	pthread_mutex_lock(&semaforoCola);
	for(x = 0; x < numeroClientes; x++){
		clientes[x].id = 0;
		clientes[x].estado = 0;
		clientes[x].posicionAtencionCliente = 0;
	}
	pthread_mutex_unlock(&semaforoCola);
	
	//Inicializo la cola de Atención al Cliente
	pthread_mutex_lock(&semaforoColaAtencionCliente);
	for(x = 0; x < numeroClientes; x++){
		atencionCliente[x].id = 0;
		atencionCliente[x].estado = 0;
		atencionCliente[x].posicionAtencionCliente = 0;
	}
	pthread_mutex_unlock(&semaforoColaAtencionCliente);
	
	
	//Creación de cajeros
	for(i = 0; i < numeroCajeros; i++){
		nuevoCajero(i);
	}
	
	while(TRUE){
	//Espera la llegada de un cliente o la finalización del programa
		pause();
	}
}

void writeLogMessage(int idCliente, int idCajero, char *msgPrimeraParte, char *msgSegundaParte, int aux){
	//Calculamos la hora actual
	time_t now = time(0);
	struct tm *tlocal = localtime(&now);
	char stnow[19];
	strftime(stnow, 19, "%d/%m/%y %H:%M:%S", tlocal);
	pthread_mutex_lock(&semaforoFichero);
	logFile = fopen(logFileName, "a");
	if(aux != -1 && idCliente == -1 && idCajero == -1){//mensaje total de clientes atendidos
		fprintf(logFile, "[%s]: %s %d.\n", stnow, msgPrimeraParte, aux);
	}else if(aux != -1 && idCliente != -1){ //mensaje cobro
		fprintf(logFile, "[%s]: %s %d ha cobrado %d %s %d.\n", stnow, msgPrimeraParte, idCajero, aux, msgSegundaParte, idCliente);
	}else if(aux != -1 && idCliente == -1){//mensaje clientes atendidos
		fprintf(logFile, "[%s]: %s %d %s %d.\n", stnow, msgPrimeraParte, idCajero, msgSegundaParte, aux);
	}else if(idCliente == -1 && idCajero == -1){//mensajes sin cajeros ni clientes
		fprintf(logFile, "[%s]: %s.\n", stnow, msgPrimeraParte);
	}else if(idCajero == -1){//mensajes solo clientes
		fprintf(logFile, "[%s]: %s %d %s.\n", stnow, msgPrimeraParte, idCliente, msgSegundaParte);
	}else if(idCliente == -1){//mensajes solo cajeros
		fprintf(logFile, "[%s]: %s %d %s.\n", stnow, msgPrimeraParte, idCajero, msgSegundaParte);
	}else{//mensajes con cajeros y clientes sin cobro
		fprintf(logFile, "[%s]: %s %d %s %d.\n", stnow, msgPrimeraParte, idCajero, msgSegundaParte, idCliente);
	}
	fclose(logFile);
	pthread_mutex_unlock(&semaforoFichero);
};

/*
*Función que calcula un número aleatorio 
*(si es entre 1 y 4 hay que pasarle 0 y 3)
*/
int calculaAleatorio (int min, int max, int id){
	time_t segundos;
  	segundos = time(NULL);
	srand(segundos * id);
	return rand() % (max - min + 1) + min + 1;
	
}


/*Con SIGUSR1 puede llegar un cliente al súper cada 
*vez que mandamos la señal.
*/

void manejadorSIGUSR1(){
	if(signal(SIGUSR1, manejadorSIGUSR1) == SIG_ERR){
		perror("llamada a signal.\n");
		exit(-1);
	}
	nuevoCliente();
}

/*Con SIGUSR2 se añade un cajero y la capacidad de
*la cola aumenta en uno.
*/

void manejadorSIGUSR2(){
	if(signal(SIGUSR2, manejadorSIGUSR2) == SIG_ERR){
	perror("llamada a SIGUSR2.\n");
	exit(-1);
	}
	char *mensajeSeIncorporaCajero = "Se incorpora el nuevo cajero";
	char *mensajeMasClientes = "y en la cola ahora cabrán los siguientes clientes";
	
	numeroClientes++;
	numeroCajeros++;
	
	writeLogMessage(numeroClientes, numeroCajeros, mensajeSeIncorporaCajero, mensajeMasClientes, -1);
	
	cajeros = (struct cajero*) realloc(cajeros, sizeof(struct cajero) * numeroCajeros);
	clientes = (struct cliente*) realloc(clientes, sizeof(struct cliente) * numeroClientes);
	atencionCliente = (struct cliente*) realloc(atencionCliente, sizeof(struct cliente) * numeroClientes);
	
	nuevoCajero(numeroCajeros - 1);
	
}


void nuevoCliente(){
	int sitioEnCola;
	pthread_mutex_lock(&semaforoCola);
	sitioEnCola = haySitioEnCola();
	pthread_mutex_unlock(&semaforoCola);
	if(sitioEnCola == -1){ //No hay sitio
		char *mensajeColaLLena = "Ha llegado un nuevo cliente pero la cola está llena";
		writeLogMessage(sitioEnCola, -1, mensajeColaLLena, "", -1);
	}else{	//Hay sitio
		pthread_mutex_lock(&semaforoCola);
		contadorClientes++;
		clientes[sitioEnCola].id = contadorClientes;
	 	pthread_mutex_unlock(&semaforoCola);
	 	pthread_create(&clientes[sitioEnCola].hiloCliente, NULL, accionesCliente, (void*) &sitioEnCola);
	 	char *mensajeSoyElCliente = "Soy el nuevo cliente";
	 	char *mensajeTengoSitio = ", he llegado a la cola y tengo sitio"; 
		writeLogMessage(clientes[sitioEnCola].id, -1, mensajeSoyElCliente, mensajeTengoSitio, -1);
	}
}

int haySitioEnCola(){
	int x;
	for(x = 0; x < numeroClientes; x++){
		if(clientes[x].id == 0){
			return x; //Encuentra posición libre
		}
	}
	return -1; //No hay sitio libre
}

int haySitioEnAtencionCliente(){
	int x;
	for(x = 0; x < numeroClientes; x++){
		if(atencionCliente[x].id == 0){
			pthread_mutex_unlock(&semaforoCola);
			return x; //Encuentra posición libre
		}
	}
	return -1; //No hay sitio libre
}

void *accionesCliente(void* sitioEnCola){
	int index = *(int *)sitioEnCola; 
	int mePuedoCansarDeEsperar = calculaAleatorio (0, 9, index); //10% de los clientes se cansan a los 10s
	int sitioAtencionClientes;
	int tiempoDeEspera = 0;
	char *mensajeSoyElCliente = "Soy el cliente";
	char *mensajeCansadoDeEsperar = "me he cansado de esperar, me voy";
	char *mensajeAdios = "adios, me voy del súper";
	char *mensajeNoSitioAtencionCliente = "he llegado a Atencion al Cliente y no hay sitio, me voy muy enfadado";
	char *mensajeAdios2 = "adios, me voy algo más calmado";
	while(TRUE){
		if(mePuedoCansarDeEsperar == 1 && tiempoDeEspera == 10){ //se cansa
			writeLogMessage(clientes[index].id, -1, mensajeSoyElCliente, mensajeCansadoDeEsperar, -1);
			pthread_mutex_lock(&semaforoCola);
			liberarSitioEnCola(index);
			pthread_mutex_unlock(&semaforoCola);
		}else{ //Espera el tiempo que sea para ser atendido
			if(clientes[index].estado == 1){//Está siendo atendido
				while (TRUE) { //Espero hasta que termine de ser atendido.
					if(clientes[index].estado == 2){
						writeLogMessage(clientes[index].id, -1, mensajeSoyElCliente, mensajeAdios, -1);
						pthread_mutex_lock(&semaforoCola);
						liberarSitioEnCola(index);
						pthread_mutex_unlock(&semaforoCola);
						pthread_exit(NULL);
					}else if(clientes[index].estado == 3){
						pthread_mutex_lock(&semaforoAtencionCliente);
						sitioAtencionClientes = haySitioEnAtencionCliente();
						pthread_mutex_unlock(&semaforoAtencionCliente);
						if(sitioAtencionClientes == -1){
							writeLogMessage(clientes[index].id, -1, mensajeSoyElCliente, mensajeNoSitioAtencionCliente, -1);
							pthread_mutex_lock(&semaforoCola);
							liberarSitioEnCola(index);
							pthread_mutex_unlock(&semaforoCola);
							pthread_exit(NULL);
						}else{
							contadorClientesAtencionCliente++;
							atencionCliente[sitioAtencionClientes].id = clientes[index].id;
							atencionCliente[sitioAtencionClientes].estado = clientes[index].estado;
							atencionCliente[sitioAtencionClientes].posicionAtencionCliente = contadorClientesAtencionCliente;
							pthread_mutex_lock(&semaforoCola);
							liberarSitioEnCola(index);
							pthread_mutex_unlock(&semaforoCola);
							while(TRUE){
								if(atencionCliente[sitioAtencionClientes].estado == 4){
									pthread_mutex_lock(&semaforoAtencionCliente);
									pthread_cond_wait(&condicionAtencionCliente, &semaforoAtencionCliente);
									pthread_mutex_unlock(&semaforoAtencionCliente);
									writeLogMessage(atencionCliente[sitioAtencionClientes].id, -1, mensajeSoyElCliente, mensajeAdios2, -1);
									pthread_mutex_lock(&semaforoColaAtencionCliente);
									liberarSitioEnAtencionCliente(sitioAtencionClientes);
									pthread_mutex_unlock(&semaforoColaAtencionCliente);
									pthread_exit(NULL);
								}
								sleep(1);
							}
						}
						
					}
					sleep(1);
				}
				
			}
		}
		tiempoDeEspera++;
		sleep(1);
	}
}

void liberarSitioEnCola(int index){
		clientes[index].id = 0;
		clientes[index].estado = 0;
}

void liberarSitioEnAtencionCliente(int index){
		atencionCliente[index].id = 0;
		atencionCliente[index].estado = 0;
		atencionCliente[index].posicionAtencionCliente = 0;
		
}

void nuevoCajero(int id){
 	cajeros[id].id = id + 1;
 	cajeros[id].contadorClientes = 0;
 	cajeros[id].descansado = 0;
 	pthread_create(&cajeros[id].hiloCajero, NULL, accionesCajero, (void*) &cajeros[id].id);
 	char *mensajeNuevoCajero = "Soy el nuevo cajero"; 
	writeLogMessage(-1, cajeros[id].id, mensajeNuevoCajero, "", -1);
}

void *accionesCajero(void* id){
	int index = *(int *)id - 1;
	int posicionSiguienteCliente;
	int tiempoAtencion;
	int hayProblemas;
	int irAAtencionCliente;
	char *mensajeElCajero = "El cajero";
	char *mensajeTomarCafe = "se va a tomar un café porque ya ha trabajado demasiado";
	char *mensajeVueltaAlTrabajo = "vuelve a su puesto de trabajo tras su merecido descanso";
	char *mensajeAtiendeA = "atiende al cliente";
	char *mensajeReponedor = "llama al reponedor porque tiene problemas con el cliente";
	char *mensajeFinReponedor = "ya ha terminado con el reponedor y va a terminar de atender al cliente";
	char *mensajeMalAtendido = "no ha podido cobrar por falta de liquidez del cliente";
	char *mensajeAtencionCliente = "manda al cliente a departamento de Atención al Cliente porque se está poniendo muy irascible";
	while(TRUE){
		if(cajeros[index].contadorClientes != 0 && cajeros[index].contadorClientes % 10 == 0 && cajeros[index].descansado == 0){//Se va a tomar un café
			cajeros[index].descansado = 1;
			writeLogMessage(-1, cajeros[index].id, mensajeElCajero, mensajeTomarCafe, -1);
			sleep(20);
			writeLogMessage(-1, cajeros[index].id, mensajeElCajero, mensajeVueltaAlTrabajo, -1);
		}
		posicionSiguienteCliente = siguienteClienteAtender();
		if(posicionSiguienteCliente != -1){
			cajeros[index].descansado = 0;
		
			//Imprimir mensaje de antención
			writeLogMessage(clientes[posicionSiguienteCliente].id, cajeros[index].id, mensajeElCajero, mensajeAtiendeA, -1);
			
			//Añadir un cliente
			cajeros[index].contadorClientes++;
			
			tiempoAtencion = calculaAleatorio(0, 4, cajeros[index].id);//De 1 a 5 segs
			sleep(tiempoAtencion);
			 
			hayProblemas = calculaAleatorio(0, 99, cajeros[index].id);//70% sin problemas, 25% reponedor, 5% no pueden terminar
			if(hayProblemas <= 70){//No hay problemas
				atenderBien(clientes[posicionSiguienteCliente].id, cajeros[index].id);
				clientes[posicionSiguienteCliente].estado = 2;
			}else if(hayProblemas <= 95){//Problema con el precio
				irAAtencionCliente = calculaAleatorio(0, 9, cajeros[index].id);
				if(irAAtencionCliente <= 5){//No atencion al cliente
					writeLogMessage(clientes[posicionSiguienteCliente].id,cajeros[index].id, mensajeElCajero, mensajeReponedor, -1);
					pthread_cond_signal(&condicionReponedor);
					pthread_mutex_lock(&semaforoReponedor);
					pthread_cond_wait(&condicionReponedor, &semaforoReponedor);
					pthread_mutex_unlock(&semaforoReponedor);
					writeLogMessage(clientes[posicionSiguienteCliente].id,cajeros[index].id, mensajeElCajero, mensajeFinReponedor, -1);
					atenderBien(clientes[posicionSiguienteCliente].id, cajeros[index].id);
					clientes[posicionSiguienteCliente].estado = 2;
				}else{ //Atencion al cliente
					writeLogMessage(clientes[posicionSiguienteCliente].id, cajeros[index].id, mensajeElCajero, mensajeAtencionCliente, -1);
					clientes[posicionSiguienteCliente].estado = 3;
				}
			}else{//No se puede realizar la compra
				irAAtencionCliente = calculaAleatorio(0, 9, cajeros[index].id);
				if(irAAtencionCliente <= 5){//No atencion al cliente
					writeLogMessage(clientes[posicionSiguienteCliente].id, cajeros[index].id, mensajeElCajero, mensajeMalAtendido, -1);
					clientes[posicionSiguienteCliente].estado = 2;
				}else{ //Atencion al cliente
					writeLogMessage(clientes[posicionSiguienteCliente].id, cajeros[index].id, mensajeElCajero, mensajeAtencionCliente, -1);
					clientes[posicionSiguienteCliente].estado = 3;
				}
			}
		}
		sleep(1);
	}
}


int siguienteClienteAtender(){
	int posicionSiguienteCliente = -1;
	int idMasBajo = contadorClientes;
	int i;
	pthread_mutex_lock(&semaforoCola);
	for(i = 0; i < numeroClientes; i++){
		 if(clientes[i].id <= idMasBajo &&  clientes[i].id != 0 && clientes[i].estado == 0){
			   posicionSiguienteCliente = i;
			   idMasBajo = clientes[i].id;
		  }
	}
	if(posicionSiguienteCliente != -1){
		clientes[posicionSiguienteCliente].estado = 1;
	}
	pthread_mutex_unlock(&semaforoCola);
	return posicionSiguienteCliente;
}

void atenderBien(int idCliente, int idCajeros){
	int precio;
	char *mensajeElCajero = "El cajero";
	char *mensajeAtendidoExito = "€ tras atender con éxito al cliente";
	precio = calculaAleatorio(0, 99, idCajeros); //Precio entre 1 y 100€
	writeLogMessage(idCliente, idCajeros, mensajeElCajero, mensajeAtendidoExito, precio);
}

void *accionesReponedor(void * nada){
	while(TRUE){
		pthread_mutex_lock(&semaforoReponedor);
		pthread_cond_wait(&condicionReponedor, &semaforoReponedor);
		pthread_mutex_unlock(&semaforoReponedor);
		int tiempoDormir = calculaAleatorio(0, 4, getpid()); //Tarda entre 1 y 5 seg en mirar el precio
		sleep(tiempoDormir);
		pthread_cond_signal(&condicionReponedor);
	}
}

void *accionesAtencionCliente(void * nada){
	char *mensajeElCliente = "El cliente";
	char *mensajeAtendiendoAtencionCliente = "está siendo calmado por el experto de Atención al Cliente";
	while(TRUE){
		int siguiente = siguienteClienteAtencionCliente();
		if(siguiente != -1){
			int tiempoDormir = calculaAleatorio(0, 4, getpid()); //Tarda entre 1 y 5 seg en mirar el precio
			writeLogMessage(atencionCliente[siguiente].id, -1, mensajeElCliente, mensajeAtendiendoAtencionCliente, -1);
			sleep(tiempoDormir);
			pthread_cond_signal(&condicionAtencionCliente);
		}
		sleep(1);
	}
}

int siguienteClienteAtencionCliente(){
	int posicionSiguienteCliente = -1;
	int llegadaMasBaja = contadorClientesAtencionCliente;
	int i;
	pthread_mutex_lock(&semaforoColaAtencionCliente);
	for(i = 0; i < numeroClientes; i++){
		 if(atencionCliente[i].posicionAtencionCliente <= llegadaMasBaja &&  atencionCliente[i].posicionAtencionCliente != 0){
			   posicionSiguienteCliente = i;
			   llegadaMasBaja = atencionCliente[i].posicionAtencionCliente;
		  }
	}
	if(posicionSiguienteCliente != -1){
		atencionCliente[posicionSiguienteCliente].estado = 4;
	}
	pthread_mutex_unlock(&semaforoColaAtencionCliente);
	return posicionSiguienteCliente;
}



void acabarPrograma(){
	if(signal(SIGINT, acabarPrograma) == SIG_ERR){
		perror("llamada a SIGINT.\n");
		exit(-1);
	}
	char *mensajeElCajero = "El cajero";
	char *mensajeClientesAtendidos = "ha atendido al siguiente número de clientes";
	char *mensajeClientesAtendidosTotales = "En total se ha atendido al siguiente número de clientes";
	int x;
	int sumaClientes = 0;
	for(x = 0; x < numeroCajeros; x++){
		writeLogMessage(-1, cajeros[x].id, mensajeElCajero, mensajeClientesAtendidos, cajeros[x].contadorClientes);
		sumaClientes = sumaClientes + cajeros[x].contadorClientes;
	}
	writeLogMessage(-1, -1, mensajeClientesAtendidosTotales, "", sumaClientes);
	free(cajeros);
	free(clientes);
	free(atencionCliente);
	exit(0);
}
