// State machine map: https://docs.google.com/presentation/d/1CsAItlWN2fxssGJggbDrWCFktAbGhIYJ73E4aiDmlCU/edit#slide=id.g6f17dd6178_0_0

// **********************************************************************
// **** TUNABLE CONSTANTS ***********************************************
// **********************************************************************
const int masterColorSwitchLengthDefault = 1200;
const int masterColorSwitchDelta = 0;
const int masterColorSwitchGapLength = 100; // Dark time in between color switches

const int lonePieceActivationMin = 1000;
const int lonePieceActivationMax = 8000;
const int masterResultDisplayLength = 1200;
const int winnerPendingWaitLength = 1200;
const int resetStateLength = 300;
const int masterSetupStateLength = 1500;
const int masterSetupDontSendGoLength = 250;
const int pipeDisplayLength = 300;
const int inputDisplayLength = 1000;
const int playerResultDisplayLength = 300;


// **********************************************************************
// **** GLOBAL VARIABLES ************************************************
// **********************************************************************

const Color playerRankColors[] = {WHITE, RED, YELLOW, GREEN, MAGENTA};
const Color masterColors[] = { RED, GREEN, BLUE , YELLOW, WHITE};


int masterColorSwitchLength = masterColorSwitchLengthDefault; // Slowly decrease

byte masterColorIndex = 0;
byte masterValue = 99;

byte adjacentMasterFace = 6;

enum playerRankValues {RANK_NONE, RANK_LOSE, RANK_MID, RANK_WIN, RANK_RESET};
const byte masterColorNum = sizeof(masterColors) / sizeof(masterColors[0]);
const byte masterValues[] = {1, 3, 6};
const byte masterValuesNum = sizeof(masterValues) / sizeof(masterValues[0]);

// Stores most recent pattern elements. Most recent is on the left.
byte lastElements[3][2] = { {99,99}, // Color index, number
                           {99,99},
                           {99,99} };

byte playerRanks[] = {6,6,6,6,6,6};
byte numPlayersAtInputTime = 0;
byte masterNextRankToAssign = 0;

byte currentPlayerRankSignal = 0;
byte currentPlayerRankCache = 0;
                    
const int lastElementsNum = sizeof(lastElements) / sizeof(lastElements[0]);

byte sendData = 1;
Timer sharedTimer;

enum overallStateValues {OS_RESET_STATE, OS_PLAYER_STATE, OS_MASTER_STATE, OS_LEAF_STATE, OS_ALONE_STATE, OS_PIPE_STATE};
byte overallState = OS_PLAYER_STATE;
enum masterStateValues {MS_SETUP_STATE, MS_SPINNER_STATE, MS_SPOONS_STATE, MS_WINNER_STATE, MS_LOSER_STATE};
byte masterState = MS_SETUP_STATE;
enum pipeStateValues {PS_IDLE_STATE, PS_ANIM_STATE};
byte pipeState = PS_IDLE_STATE;
enum aloneStateValues {AS_IDLE_STATE, AS_ACTIVE_STATE, AS_DEAD_STATE};
byte aloneState = AS_IDLE_STATE;
enum leafStateValues {LS_IDLE_STATE, LS_ANIM_STATE};
byte leafState = LS_IDLE_STATE;


enum signalStates {INERT, GO, RESOLVE, PING};
byte signalState = INERT;
Timer goBroadcast; // Used to control how long you broadcast the go signal, hopefully this helps master pick up signals while it's polling both faces
const byte goBroadcastLength = 50;
bool runGoBroadcastTimer = false; // true if still waiting for delay to finish

Timer masterColorSwitchTimer;

void setup() {
  randomize();
}

void loop() {
  updateAdjacentMasters();
  updateSignalPropagation();
  updateMasterSetupState();
  switch (overallState) {
    case OS_RESET_STATE:
      osReset();
      break;
    case OS_PLAYER_STATE:
      osPlayer();
      break;
    case OS_MASTER_STATE:
      osMaster();
      break;
    case OS_LEAF_STATE:
      osLeaf();
      break;
    case OS_ALONE_STATE:
      osAlone();
      break;
    case OS_PIPE_STATE:
      osPipe();
      break;
  }
}

void osReset() {
  if (sharedTimer.isExpired()) {
    overallState = OS_PLAYER_STATE;
  }
  if (currentPlayerRankCache < currentPlayerRankSignal) {
    currentPlayerRankCache = currentPlayerRankSignal;
  }
  if (currentPlayerRankCache == currentPlayerRankSignal && signalState == GO) {
    sharedTimer.set(resetStateLength);
  }
  setColor(MAGENTA);
}

// *****************************************************************
// ******* NON-MASTER HELPERS **************************************
// *****************************************************************


void osPlayer() {
  byte connectedFaces = 0;
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f) && shouldConsiderFace(f))
      {
        connectedFaces++;
      }
  }
  if (connectedFaces == 0 && overallState != OS_ALONE_STATE) {
    overallState = OS_ALONE_STATE;
    aloneState = 0; // reset alone state machine
    sharedTimer.set(lonePieceActivationMin + random(lonePieceActivationMax - lonePieceActivationMin));
  }
  if (connectedFaces == 1 && overallState != OS_LEAF_STATE) {
    overallState = OS_LEAF_STATE;
    leafState = 0; // reset leaf state machine
    buttonPressed(); // reset button press checker before going into leaf state
  }
  if (connectedFaces > 1 && overallState != OS_PIPE_STATE) {
    overallState = OS_PIPE_STATE;
    pipeState = 0; // reset pipe state machine
  }
}

void nonSpinnerStateChecks() {
  // evaluate neighbors
  osPlayer();
}

// *****************************************************************
// ******* LEAF STATE **********************************************
// *****************************************************************


void osLeaf() {
  nonSpinnerStateChecks();
  if (overallState != OS_LEAF_STATE) return;
  switch (leafState) {
    case LS_IDLE_STATE:
      lsIdle();
      break;
    case LS_ANIM_STATE:
      lsAnim();
      break;
  }
}

void lsIdle() {
  if (buttonPressed()) {
    signalState = GO;
    currentPlayerRankSignal = RANK_NONE;
  }
  if (signalState == GO) {
    leafState = LS_ANIM_STATE;
    sharedTimer.set(playerResultDisplayLength);
    return;
  }
  setColor(WHITE);
}

void lsAnim() {
  if (sharedTimer.isExpired()) {
    leafState = LS_IDLE_STATE;
    currentPlayerRankCache = 0;
    buttonPressed(); // reset button pressed state before going back to idle
    return;
  }
  if (currentPlayerRankCache < currentPlayerRankSignal) {
    currentPlayerRankCache = currentPlayerRankSignal;
  }
  if (currentPlayerRankCache == currentPlayerRankSignal && signalState == GO) {
    sharedTimer.set(playerResultDisplayLength);
  }
  setColor(playerRankColors[currentPlayerRankCache]);
}


// *****************************************************************
// ******* ALONE STATE *********************************************
// *****************************************************************

void osAlone() {
  nonSpinnerStateChecks();
  if (overallState != OS_ALONE_STATE) return;
  switch (aloneState) {
    case AS_IDLE_STATE:
      asIdle();
      break;
    case AS_ACTIVE_STATE:
      asActive();
      break;
    case AS_DEAD_STATE:
      // asDead();
      break;
  }
}

void asIdle() {
  if (sharedTimer.isExpired()) {
    aloneState = AS_ACTIVE_STATE;
    return;
  }
  setColor(OFF);
}

void asActive() {
  setColor(WHITE);
}

// *****************************************************************
// ******* PIPE STATE **********************************************
// *****************************************************************


void osPipe() {
  nonSpinnerStateChecks();
  if (overallState != OS_PIPE_STATE) return;
  switch(pipeState) {
    case PS_IDLE_STATE:
      psIdle();
      break;
    case PS_ANIM_STATE:
      psAnim();
      break;
  }
}

void psIdle() {
  if (signalState == GO) {
    pipeState = PS_ANIM_STATE;
    sharedTimer.set(pipeDisplayLength);
    return;
  }
  currentPlayerRankCache = 0;
  setColor(OFF);
}

void psAnim() {
  if (sharedTimer.isExpired()) {
    pipeState = PS_IDLE_STATE;
    currentPlayerRankCache = 0;
    return;
  }
  if (currentPlayerRankCache < currentPlayerRankSignal) {
    currentPlayerRankCache = currentPlayerRankSignal;
  }
  if (currentPlayerRankCache == currentPlayerRankSignal && signalState == GO) {
    sharedTimer.set(pipeDisplayLength);
  }
  setColor(playerRankColors[currentPlayerRankCache]);
}

// *****************************************************************
// ******* MASTER STATE ********************************************
// *****************************************************************

void osMaster() {
  if (masterState != MS_SETUP_STATE) {
    setValueSentOnAllFaces((INERT << 1) + 1); // master state controls its own signal states
  }
  switch (masterState) {
    case MS_SETUP_STATE:
      msSetup();
      break;
    case MS_SPINNER_STATE:
      msSpinner();
      break;
    case MS_SPOONS_STATE:
      msSpoons();
      break;
    case MS_WINNER_STATE:
      msWinner();
      break;
    case MS_LOSER_STATE:
      msLoser();
      break;
  }
}

void msSetup() {
  if(sharedTimer.isExpired()) {
    masterState = MS_SPINNER_STATE;
    currentPlayerRankSignal = RANK_NONE;
    return;
  }
  if (sharedTimer.getRemaining() > masterSetupDontSendGoLength) {
    FOREACH_FACE(f) {
      if(!isValueReceivedOnFaceExpired(f) && getSignalState(getLastValueReceivedOnFace(f)) != RESOLVE) {
        setValueSentOnFace((RANK_RESET << 3) + (GO << 1) + 1, f);
      }
      else {
        setValueSentOnFace((RESOLVE << 1) + 1, f);
      }
    }
  }
}

void msSpinner() {
  currentPlayerRankCache = 0; // Reset rank cache used to track whether to send RANK_RESET
  if (masterColorSwitchTimer.isExpired()) {  // SET NEXT MASTER COLOR
      masterColorIndex = random(masterColorNum - 1);
      masterValue = masterValues[random(masterValuesNum - 1)];
      // Shift all stored combos in lastElements to the right. TODO put this in a function ?
      for(byte i=lastElementsNum-1; i>0; i--)
      {
          lastElements[i][0] = lastElements[i-1][0];
          lastElements[i][1] = lastElements[i-1][1];
      }
      lastElements[0][0] = masterColorIndex; // Overwrite first element with newest one
      lastElements[0][1] = masterValue;

      displayCombo(masterColors[masterColorIndex], masterValue);
      masterColorSwitchTimer.set(masterColorSwitchLength);

      masterColorSwitchLength = masterColorSwitchLength - masterColorSwitchDelta; // Slowly decrease
      
  } else if (masterColorSwitchTimer.getRemaining() < masterColorSwitchGapLength) { // Blink off for a bit
      setColor(OFF);
  } 

  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)
        && getSignalState(getLastValueReceivedOnFace(f)) == GO) {
      if (getColorState(getLastValueReceivedOnFace(f)) == RANK_NONE) {
          // Received first player input
        // Check if valid hit
        bool isPlayerWin = isValidPattern();

        // initialize ranking system
        masterNextRankToAssign = 1;
        numPlayersAtInputTime = 0;
        for (byte i = 0; i < 6; i++) {
          playerRanks[i] = 7;
          if (!isValueReceivedOnFaceExpired(i)) {
            numPlayersAtInputTime++;
            playerRanks[i] = 6;
          }
        }
        if (isPlayerWin) {
          playerRanks[f] = 0;
          masterState = MS_SPOONS_STATE;
          sharedTimer.set(winnerPendingWaitLength);
        }
        else {
          playerRanks[f] = 5;
          masterState = MS_LOSER_STATE;
          sharedTimer.set(masterResultDisplayLength);
        }

        masterColorSwitchLength = masterColorSwitchLengthDefault; // reset switch length
        
        // reset pattern memory
        resetStoredPattern();

        break;
      }
      else {
        setValueSentOnFace((RESOLVE << 1) + 1, f); // if blink is still sending GO from winner/loser phase
      }
    }
  }
}

void updateSpoonsSignals() {
  FOREACH_FACE(f) {
    byte sendVal = (GO << 1) + 1;
    if(!isValueReceivedOnFaceExpired(f) && getSignalState(getLastValueReceivedOnFace(f)) != RESOLVE) {
      if (playerRanks[f] == 0) {
        sendVal = sendVal + (RANK_WIN << 3);
        setValueSentOnFace(sendVal, f);
      }
      else if (playerRanks[f] < numPlayersAtInputTime - 1) {
        sendVal = sendVal + (RANK_MID << 3);
        setValueSentOnFace(sendVal, f);
      }
      else if (playerRanks[f] >= numPlayersAtInputTime - 1 && playerRanks[f] < 6) {
        sendVal = sendVal + (RANK_LOSE << 3);
        setValueSentOnFace(sendVal, f);
      }
      else {
        setValueSentOnFace((INERT << 1) + 1, f);
      }
    }
    else {
      setValueSentOnFace((RESOLVE << 1) + 1, f);
    }
  }
}

void msSpoons() {
  byte lastPlayerRemaining = 6;
  byte playersRemaining = 0;
  if (!sharedTimer.isExpired()) {
    FOREACH_FACE(f) {
      if (playerRanks[f] == 6) {
        if (isValueReceivedOnFaceExpired(f) || getSignalState(getLastValueReceivedOnFace(f)) == GO) { // Received next player input
            playerRanks[f] = masterNextRankToAssign++;
            sharedTimer.set(winnerPendingWaitLength);
        }
        else {
          playersRemaining++;
          lastPlayerRemaining = f;
        }
      }
    }
  }
  else { // if nobody finishes the round after some time
    FOREACH_FACE(f) {
      if (playerRanks[f] == 6) {
        playerRanks[f] = 5;
      }
    }
  }

  if (playersRemaining == 1) {
    playerRanks[lastPlayerRemaining] = masterNextRankToAssign++;
    playersRemaining--;
  }
  if (playersRemaining == 0) {
    sharedTimer.set(masterResultDisplayLength); // TODO This technically isn't winner display timer because it also displays mistakes
    masterState = MS_WINNER_STATE;
  }
  setColor(OFF);
  updateSpoonsSignals();
}

void msWinner() {
  if (sharedTimer.isExpired()) {
    masterState = MS_SPINNER_STATE;
    return;
  }
  FOREACH_FACE(f) {
    setColorOnFace(masterColors[random(masterColorNum - 1)], f);
  }
  updateSpoonsSignals();
}

void msLoser() {
  if (sharedTimer.isExpired()) {
    masterState = MS_SPINNER_STATE;
    return;
  }
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f) && getSignalState(getLastValueReceivedOnFace(f)) == GO && playerRanks[f] == 6) {
      playerRanks[f] = 5;
      sharedTimer.set(masterResultDisplayLength);
      setValueSentOnFace((RESOLVE << 1) + 1, f);
    }
  }
  setColor(OFF);
  updateSpoonsSignals();
}


bool isValidPattern() {
    // Doubles
    if ((lastElements[0][0] == lastElements[1][0]) && (lastElements[0][0] != 99)) { // Check color double
        return true;
    }
    if ((lastElements[0][1] == lastElements[1][1]) && (lastElements[0][1] != 99)) { // Check value double
        return true;
    }

    return false;
}

// Sets the stored pattern to 99 (init val)
void resetStoredPattern() {
    for (byte i=0; i<lastElementsNum; i++) {
        lastElements[i][0] = 99;
        lastElements[i][1] = 99;
    }
}

void displayCombo(Color color, byte value) {
  setColor(OFF);
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

// *****************************************************************
// ******* SIGNAL PROPAGATION **************************************
// *****************************************************************

void updateSignalPropagation() {
  switch (signalState) {
    case INERT:
      inertLoop();
      break;
    case GO:
      if (runGoBroadcastTimer) { // Make sure timer runs only once
          goBroadcast.set(goBroadcastLength);
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
  sendData = (signalState << 1) + (currentPlayerRankSignal << 3);
  if (overallState == OS_MASTER_STATE) sendData += 1;
  setValueSentOnAllFaces(sendData);
}

// make color commands propagate regardless of state, reset when signal state is INERT
void comparePlayerColor(byte c) {
  if (c > currentPlayerRankSignal && !(overallState == OS_MASTER_STATE && masterState == MS_SETUP_STATE)) {
    currentPlayerRankSignal = c;
  }
}

bool shouldConsiderFace(byte f) { //if adjacent to master, should I prop signals to this face?
  if (adjacentMasterFace == 6) return true;

  if (f == adjacentMasterFace
   || f == (adjacentMasterFace + 2) % 6
   || f == (adjacentMasterFace + 3) % 6
   || f == (adjacentMasterFace + 4) % 6) {
    return true;
  }
  return false;
}

void inertLoop() {
  // when inert, default win metadata to 0
  currentPlayerRankSignal = 0;

  //listen for neighbors in GO
  FOREACH_FACE(f) {
    if (shouldConsiderFace(f) && !isValueReceivedOnFaceExpired(f)) {//a neighbor!
      if (getSignalState(getLastValueReceivedOnFace(f)) == GO) {//a neighbor saying GO!
        comparePlayerColor(getColorState(getLastValueReceivedOnFace(f)));
        signalState = GO;
        runGoBroadcastTimer = true;
      }
    }
  }
}

void goLoop() {
    signalState = RESOLVE; //I default to this at the start of the loop. Only if I see a problem does this not happen
    
    //look for neighbors who have not heard the GO news
    FOREACH_FACE(f) {
        if (shouldConsiderFace(f) && !isValueReceivedOnFaceExpired(f)) {//a neighbor!
            comparePlayerColor(getColorState(getLastValueReceivedOnFace(f)));
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
    if (shouldConsiderFace(f) && !isValueReceivedOnFaceExpired(f)) {//a neighbor!
      comparePlayerColor(getColorState(getLastValueReceivedOnFace(f)));
      if (getSignalState(getLastValueReceivedOnFace(f)) == GO) {//This neighbor isn't in RESOLVE. Stay in RESOLVE
        signalState = RESOLVE;
      }
    }
  }
}

void updateAdjacentMasters() {
  FOREACH_FACE(f) {
    // if connected and                          is adjacent master
    if (!isValueReceivedOnFaceExpired(f) && (getLastValueReceivedOnFace(f) & 1) == 1) {
      adjacentMasterFace = f;
    }
    //reset if signal is not from master when it is supposed to be
    else if (adjacentMasterFace == f) {
      adjacentMasterFace = 6;
    }
  }
}

void updateMasterSetupState() {
  if (overallState != OS_RESET_STATE
      && !(overallState == OS_MASTER_STATE && masterState == MS_SETUP_STATE)
      && currentPlayerRankSignal == RANK_RESET) {
    overallState = OS_RESET_STATE;
    signalState = GO;
    currentPlayerRankSignal = RANK_RESET;
    sharedTimer.set(resetStateLength);
  }
  // evaluate switched to master
  if (buttonMultiClicked()) {
    overallState = OS_MASTER_STATE;
    masterState = MS_SETUP_STATE;
    signalState = GO; // Don't know if this is the best place for this TODO
    currentPlayerRankSignal = RANK_RESET;
    sharedTimer.set(masterSetupStateLength);
    setColor(MAGENTA); // Change the color instantly so that the user knows the master was created. TODO Maybe should flash or rotate or something.
  }
}

byte getColorState(byte data) {
  return ((data >> 3) & 7);//returns bits C and D
}

byte getSignalState(byte data) {
  return ((data >> 1) & 3);//returns bits C and D
}
