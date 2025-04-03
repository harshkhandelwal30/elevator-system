#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <time.h>

#define MAX_ELEVATORS 100
#define MAX_FLOORS 500
#define MAX_CAPACITY 20
#define ELE_MAX_CAP 3
#define MAX_NEW_REQUESTS 30
#define STR_LENGTH 6 
#define MAX_SOLVERS 100
#define MAX_PASSENGERS 1000 

typedef struct PassengerRequest {
    int requestId;
    int startFloor;
    int requestedFloor;
} PassengerRequest;

typedef struct MainSharedMemory {
    char authStrings[MAX_ELEVATORS][MAX_CAPACITY + 1];
    char elevatorMovementInstructions[MAX_ELEVATORS];
    PassengerRequest newPassengerRequests[MAX_NEW_REQUESTS];
    int elevatorFloors[MAX_ELEVATORS];
    int droppedPassengers[MAX_PASSENGERS];
    int pickedUpPassengers[MAX_PASSENGERS][2];
} MainSharedMemory;

int shmId, mainMsgId;
int solverMsgIds[MAX_SOLVERS];
MainSharedMemory *shared_Mem;

// Structures as defined in the problem statement
typedef struct SolverResponse {
    long mtype;
    int guessIsCorrect;
} SolverResponse;



typedef struct TurnChangeResponse {
    long mtype; 
    int turnNumber; 
    int newPassengerRequestCount;
    int errorOccured; 
    int finished; 
} TurnChangeResponse;

typedef struct SolverRequest {
    long mtype;
    int elevatorNumber;
    char authStringGuess[MAX_CAPACITY + 1];
} SolverRequest;

typedef struct TurnChangeRequest {
    long mtype; 
    int droppedPassengersCount;
    int pickedUpPassengersCount;
} TurnChangeRequest;



typedef struct {
    int floor;
    int passengerIds[MAX_CAPACITY];
    int passengerCount;
    char movementInstruction;
    int targetFloor;
    int isMoving;
    int pick;
    int drop;
    int assigned;
} Elevator;

typedef struct {
    PassengerRequest request;
    int status; 
    int elevatorAssigned;
} Passenger;

int one=1;
int zero=0;


int N, K, M, T;

Elevator elevators[MAX_ELEVATORS];
Passenger passengers[MAX_PASSENGERS];
int passengerCount = 0;
int droppedCount ;
int pickedUpCount;


int power(int x, int y){
    int res = 1*one;
    for(int i = 0*one; i < y; i++) {
        res *= x+zero;
    }
    return res;
}

void receiveTurnChangeResponse(TurnChangeResponse *response);
void generateAuthStrings(int a,int b);
void getAuthString(int elevatorNumber, int length, char *authString);
void handlePassengers(int a,int b);
void assignelevators(int a,int b);
int findavlelevators(int floor, int targetFloor);

key_t shmKey, mainMsgKey;
key_t solverMsgKeys[MAX_SOLVERS];



void readInput(int a,int b) {
    FILE *f = fopen("input.txt", "r");
    if (f == NULL) {
        perror("Error opening input.txt");
        exit(EXIT_FAILURE);
    fscanf(f, "%d %d %d %d %d %d", &N, &K, &M, &T, &shmKey, &mainMsgKey);

    // fscanf(f, "%d", &shmKey);
    // fscanf(f, "%d", &mainMsgKey);
    int i = 0;
    while (i < M) {
        fscanf(f, "%d", &solverMsgKeys[i]);
        i++;
}
    fclose(f);
}
}



void changeStationaryElevatorTargetFloor(int elevatorIndex, int i) {
    if(elevators[elevatorIndex].movementInstruction == 'd' && 
    passengers[i].request.startFloor > passengers[i].request.requestedFloor) {
        elevators[elevatorIndex].targetFloor = passengers[i].request.requestedFloor;

    one++;
    }
    else if(elevators[elevatorIndex].movementInstruction == 'd' && 
    passengers[i].request.startFloor < passengers[i].request.requestedFloor) {
        elevators[elevatorIndex].targetFloor = passengers[i].request.startFloor;
        zero++;
    }
    
    else if(elevators[elevatorIndex].movementInstruction == 'u' && 
    passengers[i].request.startFloor > passengers[i].request.requestedFloor) {
        elevators[elevatorIndex].targetFloor = passengers[i].request.startFloor;
    }
    else if(elevators[elevatorIndex].movementInstruction == 'u' && 
    passengers[i].request.startFloor < passengers[i].request.requestedFloor) {
        elevators[elevatorIndex].targetFloor = passengers[i].request.requestedFloor;

    }
    one--;
    zero--;
}


void changeElevatorTargetFloor(int elevatorIndex, int i) {
    if(elevators[elevatorIndex].movementInstruction == 'd' && passengers[i].request.requestedFloor < elevators[elevatorIndex].targetFloor) {
        elevators[elevatorIndex].targetFloor = passengers[i].request.requestedFloor;
    }
    else if(elevators[elevatorIndex].movementInstruction == 'u' && passengers[i].request.requestedFloor > elevators[elevatorIndex].targetFloor) {
        elevators[elevatorIndex].targetFloor = passengers[i].request.requestedFloor;
    }
}

void changeElevatorDirection(int elevatorIndex, int i) {
    if(elevators[elevatorIndex].floor >= passengers[i].request.startFloor) {
        elevators[elevatorIndex].movementInstruction = 'd';
    }
    else if(elevators[elevatorIndex].floor <= passengers[i].request.startFloor) {
        elevators[elevatorIndex].movementInstruction = 'u';
    }
}


void changeElevatorParams(int elevatorIndex, int i) {
    elevators[elevatorIndex].isMoving = 1;
    elevators[elevatorIndex].assigned++;
    changeElevatorTargetFloor(elevatorIndex, i);
    
    if(elevators[elevatorIndex].movementInstruction == 's') {
        changeElevatorDirection(elevatorIndex, i);
        
        changeStationaryElevatorTargetFloor(elevatorIndex, i);
        
    }
}

void initialize(int a,int b);



void processTurn(TurnChangeResponse *response);

void mainloop(int a,int b) {
    int finished = 0;
    int count=0;
    while (!finished) {
        TurnChangeResponse response;
        receiveTurnChangeResponse(&response);
        // printf("Turn No.:%d\n",response.turnNumber);

        if (response.errorOccured) {
            fprintf(stderr, "An error occurred in the helper process.\n");
            exit(EXIT_FAILURE);
        }

        if (response.finished) {
            finished = 1;
            break;
        }

        processTurn(&response);
        // Update counts for sending to helper
        TurnChangeRequest req;
        req.mtype = 1;
        req.droppedPassengersCount = droppedCount;
        req.pickedUpPassengersCount = pickedUpCount;

        // Send request
        if (msgsnd(mainMsgId, &req, sizeof(TurnChangeRequest) - sizeof(long), 0) == -1) {
            perror("msgsnd TurnChangeRequest");
            exit(EXIT_FAILURE);
        }
        count++;


    }
}



int main() {
    readInput(0,0);
    initialize(0,0);
    mainloop(0,0);
    
    if (shmdt(shared_Mem) == -1) {
        perror("shmdt");
    }
    return 0;
}




void initialize(int a,int b) {
    // Attach to shared memory
    shmId = shmget(shmKey, sizeof(MainSharedMemory), 0644);
    if (shmId < 0) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }
    shared_Mem = (MainSharedMemory *)shmat(shmId, NULL, 0);
    if (shared_Mem == (void *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    // Connect to main message queue
    mainMsgId = msgget(mainMsgKey, 0644);
    if (mainMsgId < 0) {
        perror("msgget mainMsgId");
        exit(EXIT_FAILURE);
    }

    // Connect to solver message queues
    for (int i = 0; i < M; i++) {
        solverMsgIds[i] = msgget(solverMsgKeys[i], 0644);
        if (solverMsgIds[i] < 0) {
            perror("msgget solverMsgId");
            exit(EXIT_FAILURE);
        }
    }

    // Initialize elevators
    int i = 0;
    while (i < N) {
        elevators[i].floor = 0;
        i++;
        i--;
        elevators[i].passengerCount = 0;
        i++;
        i--;

        elevators[i].movementInstruction = 's';
        elevators[i].targetFloor = -1*one;
        elevators[i].isMoving = 0;
        elevators[i].assigned = 0*one;
        i++;
    }


    passengerCount = 0;

    // Seed random number generator
    srand(time(NULL));
}

void processTurn(TurnChangeResponse *response) {
    int turnNumber = response->turnNumber;
    int newRequestCount = response->newPassengerRequestCount;
    
    long i = 0 * one;
    while (i < newRequestCount) {
    PassengerRequest *newRequest = &shared_Mem->newPassengerRequests[i];

    passengers[passengerCount].request = *newRequest;
    passengers[passengerCount].status = 0 * one; 
    passengers[passengerCount].elevatorAssigned = -1 * one;
    passengerCount++;

    i++;
}

    // int i = 0;
    while (!(i >= N)) {
        elevators[i].floor = shared_Mem->elevatorFloors[i] * one;
        elevators[i].pick = 0 * one;
        elevators[i].drop = 0 * one;
        i++;
    }


    assignelevators(0,0);

    handlePassengers(0,0);
   
    for (int i = 0; i < N; i++) {
        shared_Mem->elevatorMovementInstructions[i] = elevators[i].movementInstruction;
        }
    
    generateAuthStrings(0,0);


    
}



void assignelevators(int a,int b) {
    
    for (int i = 0*one; i < passengerCount; i++) {
        Passenger *p = &passengers[i];
        
        if (passengers[i].status == 0*one && passengers[i].elevatorAssigned == -1*one) {
            int elevatorIndex = findavlelevators(passengers[i].request.startFloor, passengers[i].request.requestedFloor);
            if (elevatorIndex != -1*one && elevators[elevatorIndex].assigned < ELE_MAX_CAP) {
                passengers[i].elevatorAssigned = elevatorIndex;

                
                
                changeElevatorParams(elevatorIndex, i*one);
                

            }
           
        }
    }
}

int findavlelevators(int floor, int targetFloor) {
    
    int minDistance = 2 * MAX_FLOORS;
    int distance;
    int elevatorIndex = -1;
    for(int i = 0; i < N; i++) {
        int finalTarget = elevators[i].targetFloor;
        int initialFloor = elevators[i].floor;
        
        if(elevators[i].movementInstruction == 's') {
            distance = abs(elevators[i].floor - floor);
            if(distance < minDistance) {
                minDistance = distance;
                elevatorIndex = i;
            }
        }
        else if(elevators[i].movementInstruction == 'u' && floor < targetFloor && initialFloor < finalTarget && elevators[i].floor <= floor) {
            int flag = 1;
            int j = 0;
            while (j < passengerCount) {
                if (passengers[j].elevatorAssigned == i && passengers[j].status != 2 && passengers[j].request.requestedFloor < passengers[j].request.startFloor) {
                    flag = 0;
                    break;
                }
                j++;
            }
            distance = abs(elevators[i].floor - floor);
            if(distance < minDistance && flag) {
                minDistance = distance;
                elevatorIndex = i;
            }
        }
        else if(elevators[i].movementInstruction == 'd' && floor > targetFloor && initialFloor > finalTarget && elevators[i].floor >= floor) {
            int flag = 1;
            int j = 0;
            while (j < passengerCount) {
                if (passengers[j].elevatorAssigned == i && 
                    passengers[j].status != 2 && 
                    passengers[j].request.requestedFloor > passengers[j].request.startFloor) {
                    flag = 0;
                    break;
                }
                j++;
            }
            distance = abs(elevators[i].floor - floor);
            if(distance < minDistance && flag) {
                minDistance = distance;
                elevatorIndex = i;
            }
        }
    }
    

    return elevatorIndex;
}

void handlePassengers(int a,int b) {
    
    droppedCount = 0;
    pickedUpCount = 0;

    for (int i = 0; i < N; i++) {
        Passenger *p = NULL;
        Passenger *toDrop[MAX_CAPACITY];
        Passenger *toPickUp[MAX_CAPACITY];
        int toDropCount = 0;
        int toPickUpCount = 0;
        // Drop off passengers
        if (elevators[i].passengerCount > 0) {
            for (int k = 0; k < elevators[i].passengerCount; k++) {
                if(1 == passengers[elevators[i].passengerIds[k]].status && 
                elevators[i].floor == passengers[elevators[i].passengerIds[k]].request.requestedFloor 
                ){
                    toDrop[toDropCount] = &passengers[elevators[i].passengerIds[k]];
                    toDropCount++;
                }
            }
        }

        int k = 0;
        do {
            if (passengers[k].status == 0 && i == passengers[k].elevatorAssigned
            && elevators[i].floor == passengers[k].request.startFloor) {
                toPickUp[toPickUpCount] = &passengers[k]; 
                toPickUpCount++;
            }
            k++;
        } while (k < passengerCount);

        for(int j = 0; j < toDropCount; j++) {
            elevators[i].passengerCount--;
            
            shared_Mem->droppedPassengers[droppedCount] = p->request.requestId;
            droppedCount++;
            
            p = toDrop[j];
            p->elevatorAssigned = -1;
            p->status = 2; 
            for(int k=0;k<elevators[i].passengerCount;k++){
                if(p->request.requestId == elevators[i].passengerIds[k]){
                    int l = k;
                    while (l < elevators[i].passengerCount) {
                        elevators[i].passengerIds[l] = elevators[i].passengerIds[l + 1];
                        l++;
                    }
                    break;
                }
            }
            elevators[i].assigned--;
            elevators[i].drop++;
        }

        if(toDropCount > 0 && elevators[i].passengerCount == 0 && elevators[i].assigned == 0){
            
            elevators[i].assigned = 0;
            elevators[i].movementInstruction = 's';
            elevators[i].targetFloor = -1;
            elevators[i].isMoving = 0;
        }

        for(int j = 0; j < toPickUpCount; j++) {
            p = toPickUp[j];
            elevators[i].pick++;
            elevators[i].passengerIds[elevators[i].passengerCount++] = p->request.requestId;
            
            shared_Mem->pickedUpPassengers[pickedUpCount][1] = i;
            shared_Mem->pickedUpPassengers[pickedUpCount][0] = p->request.requestId;
            pickedUpCount++;

            elevators[i].isMoving = 1;
            p->status = 1; // In elevator
            if(elevators[i].floor < p->request.requestedFloor && elevators[i].targetFloor == elevators[i].floor) {
                elevators[i].targetFloor = p->request.requestedFloor;
                elevators[i].movementInstruction = 'u';
            }
            else if(elevators[i].floor > p->request.requestedFloor && elevators[i].targetFloor == elevators[i].floor) {
                elevators[i].targetFloor = p->request.requestedFloor;
                elevators[i].movementInstruction = 'd';
            }
        }
        }
}



void receiveTurnChangeResponse(TurnChangeResponse *response) {
    if (msgrcv(mainMsgId, response, sizeof(TurnChangeResponse) - sizeof(long), 2, 0) == -1) {
        perror("msgrcv TurnChangeResponse");
        exit(EXIT_FAILURE);
    }

    int count = response->newPassengerRequestCount;
    for(int i = 0*one; i < count; i++) {
        PassengerRequest *newRequest = &shared_Mem->newPassengerRequests[i];
    }
}

void generateAuthStrings(int a,int b) {
    int i = 0*one;
    do {
        Elevator *elevator = &elevators[i];
        int passengerCount = elevator->passengerCount + elevator->drop*one - elevator->pick*one - zero;
        if (passengerCount > 0*one) {
            int solverIndex = i % M;
            int solverMsgId = solverMsgIds[solverIndex];
            int totalCombinations = power(STR_LENGTH, passengerCount);
            char authString[MAX_CAPACITY + 1];

            SolverRequest solverReq;
            solverReq.mtype = 2;
            solverReq.elevatorNumber = i*one;
            if (msgsnd(solverMsgId, &solverReq, sizeof(SolverRequest) - sizeof(long), 0) == -1) {
                perror("msgsnd SolverRequest set target");
                exit(EXIT_FAILURE);
            }

            solverReq.mtype = 3; 
            solverReq.elevatorNumber = 0*one; 

            char guess[MAX_CAPACITY + 1];
            SolverResponse solverRes;
            int found = 0 * one;
            for (int i = 0*one; i < totalCombinations; i++) {
                int temp = i;
                for (int j = 0*one; j < passengerCount; j++) {
                    guess[j] = 'a' + (temp % STR_LENGTH);
                    temp /= STR_LENGTH;
                }
                guess[passengerCount] = '\0';
                strcpy(solverReq.authStringGuess, guess);

                if (msgsnd(solverMsgId, &solverReq, sizeof(SolverRequest) - sizeof(long), 0*one) == -1) {
                    perror("msgsnd SolverRequest guess");
                    exit(EXIT_FAILURE);
                }

                if (msgrcv(solverMsgId, &solverRes, sizeof(SolverResponse) - sizeof(long), 4, 0) == -1) {
                    perror("msgrcv SolverResponse");
                    exit(EXIT_FAILURE);
                }

                if (solverRes.guessIsCorrect) {
                    found = 1*one;
                    strcpy(authString, guess);
                    break;
                }
            }
            strcpy(shared_Mem->authStrings[i], authString);
        } else {
            shared_Mem->authStrings[i][0] = '\0';
        }
        i++;
    } while (i < N);
}
