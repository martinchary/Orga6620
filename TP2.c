#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
// el cache tiene 8 vías, 16Kb de capacidad y bloques de 64 bytes que almacenan
// direcciones de 16 bits.
// entonces, cuenta con 32 conjuntos por via, que pueden almacenar 32 palabras cada uno 
//tenemos una memoria de 64Kb. Con bloques de 64 bytes cada uno, son 1024 bloques en memoria.

// Configuracion del cache
#define dirSpace 16  //bits por palabra
#define blockSize 32 //palabras de 16 bits por cada bloque
#define Nways 8      //numero de vias de cache
#define Nsets 32     //numero de conjuntos de cache
#define Nmem 1024    //numero de bloques de memoria


//las direcciones de memoria se almacenan:

//        tag (6 bits)   offset (5bits)
//            |----|     |---|
// (1A3E) =   0001101000111110
//                  |---|
//              index (5 bits)

//	int offsetBits = log2(blockSize);
//	int indexBits = log2(Nsets);
//  int tagBits = dirSpace - log2(blockSize)- log2(Nsets);

typedef struct
{
	int dir[blockSize];
	int tag, v, order;
} CacheBlock;

typedef struct
{
	int dir[blockSize];
} MemBlock;


MemBlock newMBlock()
{
	//cada una de las 32 direcciones de memoria contiene valores aleatorios entre 0 y 2^16
	int MaxValue = 65536;
	MemBlock nb;
	int b;
	for (b=0; b < blockSize; b++)
	{
		nb.dir[b] = rand()% MaxValue;
	}
	return nb;
}

CacheBlock newBlock()
{
	CacheBlock nb;
	int b;
	for (b=0; b < blockSize; b++)
	{
		nb.dir[b] = 0;
	}
	nb.tag = 0;
	nb.v=0;
	nb.order=0;
	return nb;
}

//VARIABLES GLOBALES
// hits, misses, cache y memoria

int hits = 0;
int misses = 0;
CacheBlock cache[Nways][Nsets];
MemBlock memory[Nmem];

//FUNCIONES

void init()
{
	//inicializo hits and misses
	hits = 0;
	misses = 0;
	//inicializo cache en 0
	int a, b;
	for (a = 0; a<Nways; a++)
	{
		for (b =0; b < Nsets; b++)
		{
			cache[a][b] = newBlock();
		}
	}
	//inicializo los 1024 bloques de memoria
	for (a = 0; a<Nmem; a++)
	{
		memory[a] = newMBlock();
	}
}

unsigned int get_index(unsigned int address)
{
	//dada una address obtiene su indice
	//tiene 5 bits de offset y 5 bits de indice
	//saco resultado entero de dividir por 32, y luego saco el resto de dividir por 32.
	return address/ blockSize % Nsets;
}

unsigned int get_offset(unsigned int address)
{
	//retorna offset de una direccion
	//tiene 5 bits de offset (porque el tamanio de bloque es de 64 bytes,
	//y cada linea tiene 32 bloques), entonces saco resto de dividir por 32
	return address % blockSize;
}

unsigned int get_tag(unsigned int address)
{
	//retorna tag de una direccion.
	//como tiene espacio de direcciones de 16 bits, y hay 5 bits de offset
	//y 5 de index, quedan 6 bits de tag. O sea, divido por 32*32
	return address / (blockSize*Nsets);
}

unsigned int find_set(unsigned int address)
{
	//toma una direccion de memoria e indica a que set del cache encaja
	return get_index(address);
}

unsigned int select_oldest(unsigned int setnum)
{
	//retorna la posicion con mayor antiguedad en un conjunto,
	//o la primera tenga bit de validez 0 (vacia)
	int maxValue = 0;
	int maxPos = 0;
	int x;
	int y;
	for (x =0; x < Nways; x++)
	{
		if (cache[x][setnum].v == 0)
			{
				maxPos = x;
				break;
			}
		else
		{
			y = cache[x][setnum].order;
			if (maxValue < y)
			{
				maxValue = y;
				maxPos = x;
			}
		}

	}
	return maxPos;
}

void increase_order(unsigned int set)
{
	//sube el valor de antiguedad para todo el set que tenga bit de validez en 1
	int i;
	for (i=0; i < Nways; i++)
	{
		if (cache[i][set].v == 1)
		{
			cache[i][set].order++;
		}
	}
}

void read_tocache(unsigned int blocknum, unsigned int way, unsigned int set)
{
	//copia un bloque de memoria a cache
	int i =0;
	for (i = 0; i <blockSize; i++)
	{
		cache[way][set].dir[i] = memory[blocknum].dir[i];
	}
	cache[way][set].tag = blocknum;
	cache[way][set].v = 1;
	cache[way][set].order = 0;
}

signed int tag_in_set(unsigned int tag, unsigned int index)
{
	//revisa si un tag esta en el set dado un index
	//si esta, retorna en que via.
	int check = -1;
	int i = 0;
	for (i = 0; i< Nways; i++)
	{
		if (cache[i][index].tag == tag && cache[i][index].v == 1)
		{
			check = i;
			break;
		}
	}
	return check;
}

unsigned char read_byte(unsigned int address)
{
	int AIndex = get_index(address);
	int ATag = get_tag(address);
	int AOffset = get_offset(address);
	//reviso si el address esta en cache.
	//voy al set que contiene el indice del address a ver si contiene el tag del address
	//en alguna de las 8 vias.
	if (tag_in_set(ATag, AIndex) == -1)
	{
		//Si el tag no coincide hay un miss, le pido que haga un fetch.
		//1)incremento order para todos en el set con bit de validez 1
		increase_order(AIndex);
		//2) copio el bloque de memoria a la posicion indicada
		read_tocache(ATag, select_oldest(AIndex), AIndex);
		misses++;
	}
	else
	{
		//si el tag coincide es un hit
		hits++;
	}
	//ahora si o si esta el byte en cache. Veo en que posicion esta:
	int pos = tag_in_set(ATag, AIndex);
	return cache[pos][AIndex].dir[AOffset];
}

void write_byte(unsigned int address, unsigned char value)
{
	int AIndex = get_index(address);
	int ATag = get_tag(address);
	int AOffset = get_offset(address);
	//el cache es WB/WA, por lo tanto escribe si o si en memoria
	memory[ATag].dir[AOffset] = value;
	//veo si el address esta en cache
	if (tag_in_set(ATag, AIndex) == -1)
	{
		//si el tag no esta hay un miss.
		misses++;
	}
	else
	{
		//si el tag esta hay un hit.
		hits++;
		//escribo en esta direccion
		int pos = tag_in_set(ATag, AIndex);
		cache[pos][AIndex].dir[AOffset] = value;
	}
}

float get_miss_rate()
{
	//devuelve 0 si no se realizo ninguna operacion, el miss rate en caso contrario
	float missRate = 0;
	int total = hits + misses;
	if (total != 0)
	{
		missRate =(float)misses/(float)total;
	}
	return missRate;
}

void delete_char(char *s,char c)
{
	//uso para parsear los strings con instrucciones
	int i,k=0;
	for(i=0;s[i];i++)
	{
		s[i]=s[i+k];
		if(s[i]==c)
		{
			k++;
			i--;
		}
	}
}

void decode_instruction(char line[256])
{
	if(strstr(line, "FLUSH ") != NULL)
	{
		//ante flush, inicializo el cache, memoria y hits/misses
        init();
	}
	if(strstr(line, "R ") != NULL)
	{
		//parseo la linea
		char * i = strtok(line, " " );
		i = strtok(NULL, " " );
        delete_char(i, '\n');
        int x = strtol(i, (char **)NULL, 10);
        //leo el int x
        read_byte(x);
        //printf("R %d \n", x); 
	}
	if(strstr(line, "W ") != NULL)
	{
		//parseo la linea
		char * i = strtok(line, " " );
		i = strtok(NULL, " " );
		char * j = strtok(NULL, "," );
		delete_char(i, ','); 
        delete_char(j, ' '); 
        delete_char(j, '\n'); 
        int x = strtol(i, (char **)NULL, 10);
        int y = strtol(j, (char **)NULL, 10);

        //escribo el byte y en la posicion x
        write_byte(x, y);

        //printf("W %d %d \n", x, y);
	}
	if(strstr(line, "MR") != NULL)
	{
		printf("Miss rate: %f \n", get_miss_rate());
	}
}

int main(int argc, char* argv[])
{
	srand(time(0));
	init();

	//leo el texto pasado como argumento

    char const* const fileName = argv[1];
    FILE* file = fopen(fileName, "r");
    char line[256];

    while (fgets(line, sizeof(line), file)) {
        decode_instruction(line);
    }

    fclose(file);

	return 0;
}