enum signalStates {INERT, GO, RESOLVE};
byte signalState = INERT;

unsigned long blinkDelayStart;
unsigned long currentTime;
//unsigned long delayStart = 0; // Used to control master's pattern switching
Timer goBroadcast; // Used to control how long you broadcast the go signal, hopefully this helps master pick up signals while it's polling both faces

Timer masterColorSwitchTimer;
int masterColorSwitchLengthDefault = 2000;
int masterColorSwitchDelta = 0;
int masterColorSwitchLength = masterColorSwitchLengthDefault; // Slowly decrease

#define OFF_DURATION 100

Timer winnerDisplayLengthTimer;
const int winnerDisplayLength = 3000;
bool clearedWinner = false;

bool runGoBroadcastTimer = false; // true if still waiting for delay to finish

enum gameModes {MODE1, MODE2, MODE3, MASTER_MODE, BECOME_MASTER};//these modes will simply be different colors (editor's note: not quite true anymore)
byte gameMode = MODE1;//the default mode when the game begins

int masterColorIndex = 0;
int masterValue = 99;

const Color masterColors[] = { RED, GREEN, BLUE , YELLOW, WHITE};
const int masterColorNum = sizeof(masterColors) / sizeof(masterColors[0]);
const int masterValues[] = {1, 3, 6};
const int masterValuesNum = sizeof(masterValues) / sizeof(masterValues[0]);

int masterNeighbors[6] = { 0, 1, 2, 3, 4, 5 }; // Used to speed up checking if we received an input so we don't have to check all faces, not sure if necessary.
int neighborNum = sizeof(masterNeighbors) / sizeof(masterNeighbors[0]);

bool isMaster = false;

// Stores most recent pattern elements. Most recent is on the left.
//int lastElements[3] = {99, 99, 99}; // TODO not sure if good initialization. Can't store colors directly because there's no equals operator defined. 

int lastElements[3][2] = { {99,99}, // Color index, number
                           {99,99},
                           {99,99} };
                    
const int lastElementsNum = sizeof(lastElements) / sizeof(lastElements[0]);

void setup() {
  randomize();
}

void masterInit() {
    gameMode = MASTER_MODE; 
    signalState = RESOLVE; // Don't know if this is the best place for this TODO

    setColor(RED); // Change the color instantly so that the user knows the master was created. TODO Maybe should flash or rotate or something.

    // Determine which two sides are the inputs to master
//    FOREACH_FACE(f) {
//        if (!isValueReceivedOnFaceExpired(f)) {//a neighbor!
//            if (masterNeighbors[0] == 9) {
//                masterNeighbors[0] = f;
//            } else {
//                masterNeighbors[1] = f;
//            }
//        }
//    }
}

void loop() {
    if (isMaster) {
        masterLoop();
    } else {
        switch (signalState) {
          case INERT:
            inertLoop();
            break;
          case GO:
            if (runGoBroadcastTimer) { // Make sure timer runs only once
                goBroadcast.set(200);
            }
            if (!goBroadcast.isExpired()) {
                // Keep broadcasing GO for a time
                runGoBroadcastTimer = false;
            } else {
                goLoop();
            }
            break;
          case RESOLVE:
            resolveLoop();
            break;
        }
    }
    displaySignalState();
  
    byte sendData = (signalState << 2) + (gameMode);
    setValueSentOnAllFaces(sendData);
  
    if (buttonLongPressed()) {
        isMaster = !isMaster;
        if (isMaster) {
            masterInit();
        }
    }
}

bool isValidPattern() {
    // Doubles
    if ((lastElements[0][0] == lastElements[1][0]) && (lastElements[0][0] != 99)) { // Check color double
        return true;
    }
    if ((lastElements[0][1] == lastElements[1][1]) && (lastElements[0][1] != 99)) { // Check value double
        return true;
    }
    //for(int i=0; i<lastElementsNum; i++)

    return false;
}

void setMasterResult(int inputFace, bool isCorrect) {
    Color inputColor = GREEN; // TODO Maybe CYAN
    if (!isCorrect) {
        inputColor = RED;
    }
    // Color on side input was received from
    setColorOnFace(inputColor, (inputFace + 1) % 6);
    setColorOnFace(inputColor, inputFace);
    setColorOnFace(inputColor, (inputFace + 5) % 6);

    // Color on opposite side
    setColorOnFace(OFF, (inputFace + 2) % 6);
    setColorOnFace(OFF, (inputFace + 3) % 6);
    setColorOnFace(OFF, (inputFace + 4) % 6);
}

// Sets the stored pattern to 99 (init val)
void resetStoredPattern() {
    for (int i=0; i<lastElementsNum; i++) {
        lastElements[i][0] = 99;
        lastElements[i][1] = 99;
    }
}

void displayCombo(Color color, int value) {
    if (value >= 1) {
        setColorOnFace(color, 0);
    }
    if (value >= 3) {
        setColorOnFace(color, 2);
    }
    if (value >= 6) {
        setColorOnFace(color, 4);
    }
}

void masterLoop() {
    if (winnerDisplayLengthTimer.isExpired()) { // This timer controls how long we display the winning state, prevents reading new inputs

        if (!clearedWinner) {
            setColor(OFF);
            clearedWinner = true;
        }
        
        if (masterColorSwitchTimer.isExpired()) {  // SET NEXT MASTER COLOR
            masterColorIndex = random(masterColorNum - 1);
            masterValue = masterValues[random(masterValuesNum - 1)];
            // Shift all stored combos in lastElements to the right. TODO put this in a function ?
            for(int i=lastElementsNum-1; i>0; i--)
            {
                lastElements[i][0] = lastElements[i-1][0];
                lastElements[i][1] = lastElements[i-1][1];
            }
            lastElements[0][0] = masterColorIndex; // Overwrite first element with newest one
            lastElements[0][1] = masterValue;

            displayCombo(masterColors[masterColorIndex], masterValue);
            masterColorSwitchTimer.set(masterColorSwitchLength);

            masterColorSwitchLength = masterColorSwitchLength - masterColorSwitchDelta; // Slowly decrease
            
        } else if (masterColorSwitchTimer.getRemaining() < OFF_DURATION) { // Blink off for a bit
            setColor(OFF);
        } 
    
        //iterate over master's (assumed to be) 2 neighbors
        int neighborIndex = 0;
        for (neighborIndex; neighborIndex<neighborNum; neighborIndex++) {
            int neighborFace = masterNeighbors[neighborIndex];
            if (getSignalState(getLastValueReceivedOnFace(neighborFace)) == GO) { // Received first player input
                // Check if valid hit TODO
                bool isPlayerWin = isValidPattern();

                // Set winning side to GREEN and losing side to OFF. If wrong set first side to RED.
                setMasterResult(neighborFace, isPlayerWin);

                int masterColorSwitchLength = masterColorSwitchLengthDefault; // Slowly decrease
                
                // reset pattern memory
                resetStoredPattern();
    
                winnerDisplayLengthTimer.set(winnerDisplayLength); // TODO This technically isn't winner display timer because it also displays mistakes
                clearedWinner = false;
                
                // byte becomeMasterPayload = (INERT << 2) + (BECOME_MASTER);
                
                //if (isPlayerWin) {
                //    setValueSentOnFace(becomeMasterPayload, masterNeighbors[ (neighborIndex + 1 % 2) ] );
                //} else {
                //    setValueSentOnFace(becomeMasterPayload, neighborFace);
                //}

                //isMaster = false;
                
                break;
            }
        }
//        if (neighborIndex>=2) { // Fix OFF bug?
//            displayCombo(masterColors[masterColorIndex], masterValue);
//        }
    }
}

void inertLoop() {
  //set myself to GO
//  if (buttonSingleClicked()) {
  if (buttonDown()) {
    signalState = GO;
    runGoBroadcastTimer = true;
    //change game mode
    gameMode = (gameMode + 1) % 3;//adds one to game mode, but 3+1 becomes 0
  }

  //listen for neighbors in GO
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {//a neighbor!
      if (getSignalState(getLastValueReceivedOnFace(f)) == GO) {//a neighbor saying GO!
        signalState = GO;
        runGoBroadcastTimer = true;
        gameMode = getGameMode(getLastValueReceivedOnFace(f));
        //if (gameMode == BECOME_MASTER) {
        //  isMaster = true;
        //  masterInit();
        //}
      }
    }
  }
}

void goLoop() {
    signalState = RESOLVE; //I default to this at the start of the loop. Only if I see a problem does this not happen
    
    //look for neighbors who have not heard the GO news
    FOREACH_FACE(f) {
        if (!isValueReceivedOnFaceExpired(f)) {//a neighbor!
            if (getSignalState(getLastValueReceivedOnFace(f)) == INERT) {//This neighbor doesn't know it's GO time. Stay in GO
                signalState = GO;
            }
        }
    }
}

void resolveLoop() {
  signalState = INERT;//I default to this at the start of the loop. Only if I see a problem does this not happen

  //look for neighbors who have not moved to RESOLVE
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {//a neighbor!
      if (getSignalState(getLastValueReceivedOnFace(f)) == GO) {//This neighbor isn't in RESOLVE. Stay in RESOLVE
        signalState = RESOLVE;
      }
    }
  }
}

void displaySignalState() {
  switch (signalState) {
    case INERT:
      switch (gameMode) {
        case MODE1:
          setColor(OFF);
          break;
        case MODE2:
          setColor(OFF);
          break;
        case MODE3:  // TODO remove these
          setColor(OFF); 
          break;
//
////        case MASTER_MODE:
////          setColor(CYAN);
////          break;
      }
      break;
    case GO:
      setColor(MAGENTA);
    case RESOLVE:
      if (!isMaster) {
        setColor(WHITE);
      }
      break;
  }
}

byte getGameMode(byte data) {
  return (data & 3);//returns bits E and F
}

byte getSignalState(byte data) {
  return ((data >> 2) & 3);//returns bits C and D
}
