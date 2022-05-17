//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include <math.h>
#include "predictor.h"


//
// TODO:Student Information
//
const char *studentName = "Danlin Jiang";
const char *studentID   = "A15380649";
const char *email       = "d1jiang@ucsd.edu";

//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//

// Handy Global for use in output routines
const char *bpName[4] = { "Static", "Gshare",
                          "Tournament", "Custom" };

//define number of bits required for indexing the BHT here. 
int ghistoryBits = 14; // Number of bits used for Global History
int tour_ghistoryBits = 12; // FIXME:(12 bits of GHR) Number of bits used for global history (tournament)
int tour_lhistoryBits = 10; // FIXME:(10 bits of local history entry) Number of bits used for local history (tournament)
int bpType;       // Branch Prediction Type
int verbose;

//perceptron
// int perc_wBits = 12; //FIXME: find out the best number of weight length
int perc_ghistoryBits = 16; //FIXME: find out the best number of weight length
int thred = 40; //FIXME

#define SL 0
#define WL 1
#define WG 2
#define SG 3


//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

//
//TODO: Add your own Branch Predictor data structures here
//
//gshare
uint8_t *bht_gshare;
uint64_t ghistory;
//tour
uint8_t *pht_local;
uint8_t *bht_local;
uint8_t *chooser;
//perceptron
int **perc_table;
uint64_t perc_ghistory;


//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor
//

//gshare functions
void init_gshare() {
 int bht_entries = 1 << ghistoryBits;
  bht_gshare = (uint8_t*)malloc(bht_entries * sizeof(uint8_t));
  int i = 0;
  for(i = 0; i< bht_entries; i++){
    bht_gshare[i] = WN;
  }
  ghistory = 0;
}



uint8_t 
gshare_predict(uint32_t pc) {
  //get lower ghistoryBits of pc
  uint32_t bht_entries = 1 << ghistoryBits;
  uint32_t pc_lower_bits = pc & (bht_entries-1);
  uint32_t ghistory_lower_bits = ghistory & (bht_entries -1);
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;
  switch(bht_gshare[index]){
    case WN:
      return NOTTAKEN;
    case SN:
      return NOTTAKEN;
    case WT:
      return TAKEN;
    case ST:
      return TAKEN;
    default:
      printf("Warning: Undefined state of entry in GSHARE BHT!\n");
      return NOTTAKEN;
  }
}

void
train_gshare(uint32_t pc, uint8_t outcome) {
  //get lower ghistoryBits of pc
  uint32_t bht_entries = 1 << ghistoryBits;
  uint32_t pc_lower_bits = pc & (bht_entries-1);
  uint32_t ghistory_lower_bits = ghistory & (bht_entries -1);
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;

  //Update state of entry in bht based on outcome
  switch(bht_gshare[index]){
    case WN:
      bht_gshare[index] = (outcome==TAKEN)?WT:SN;
      break;
    case SN:
      bht_gshare[index] = (outcome==TAKEN)?WN:SN;
      break;
    case WT:
      bht_gshare[index] = (outcome==TAKEN)?ST:WN;
      break;
    case ST:
      bht_gshare[index] = (outcome==TAKEN)?ST:WT;
      break;
    default:
      printf("Warning: Undefined state of entry in GSHARE BHT!\n");
  }

  //Update history register
  ghistory = ((ghistory << 1) | outcome); 
}

void
cleanup_gshare() {
  free(bht_gshare);
}


//start_tournament
void init_tournament() {
  //local predictor
  int pht_entries = 1 << tour_lhistoryBits;
  pht_local = (uint8_t*)malloc(pht_entries * sizeof(uint8_t));
  bht_local = (uint8_t*)malloc(pht_entries * sizeof(uint8_t));
  //gshare
  int bht_entries = 1 << tour_ghistoryBits;
  bht_gshare = (uint8_t*)malloc(bht_entries * sizeof(uint8_t));
  int i = 0;
  //chooser
  int chooser_entries = 1 << tour_ghistoryBits;
  chooser = (uint8_t*)malloc(chooser_entries * sizeof(uint8_t));
  for(i = 0; i< bht_entries; i++){
    bht_gshare[i] = WN;
    chooser[i] = WL;
  }
  for(i = 0; i<pht_entries; i++){
    pht_local[i] = WN;
  }
  ghistory = 0;//GHR
}



uint8_t 
tournament_predict(uint32_t pc) {
  //local
  uint32_t l_bht_entries = 1 << tour_lhistoryBits;//2^10
  uint32_t l_pc_lower_bits = pc & (l_bht_entries-1);

  //gshare
  uint32_t g_bht_entries = 1 << tour_ghistoryBits;//2^12
  uint32_t g_pc_lower_bits = pc & (g_bht_entries-1);//TODO: why -1?
  uint32_t g_ghistory_lower_bits = ghistory & (g_bht_entries -1);
  uint32_t index = g_pc_lower_bits ^ g_ghistory_lower_bits;
  uint32_t choice;
  //get chooser result
  switch(chooser[g_pc_lower_bits]){
    case SL:
    case WL:
      choice = bht_local[pht_local[l_pc_lower_bits]];
      break;
    case WG:
    case SG:
      choice = bht_gshare[index];
      break;
    default:
      printf("Warning: Undefined state of entry in TOUR chooser!\n");
      return NOTTAKEN;
  }

  switch(choice){
        case WN:
          return NOTTAKEN;
        case SN:
          return NOTTAKEN;
        case WT:
          return TAKEN;
        case ST:
          return TAKEN;
        default:
          printf("Warning: Undefined state of entry in GSHARE/LOCAL BHT! (TOUR)\n");
          return NOTTAKEN;
      }
  
}

void
train_tournament(uint32_t pc, uint8_t outcome) {
  //train local
  uint32_t l_bht_entries = 1 << tour_lhistoryBits;//2^10
  uint32_t l_pc_lower_bits = pc & (l_bht_entries-1);

  //train gshare
  //get lower ghistoryBits of pc
  uint32_t g_bht_entries = 1 << tour_ghistoryBits;
  uint32_t g_pc_lower_bits = pc & (g_bht_entries-1);
  uint32_t ghistory_lower_bits = ghistory & (g_bht_entries -1);
  uint32_t index = g_pc_lower_bits ^ ghistory_lower_bits;

  //train chooser
  //chooser stay the same if both predictor takes the same choice
  //only check the bit at left
  if((bht_local[pht_local[l_pc_lower_bits]]>>1)!=(bht_gshare[index]>>1)){
    switch(chooser[g_pc_lower_bits]){
      case SL:
        chooser[g_pc_lower_bits] = (outcome==(bht_gshare[index]>>1))?WL:SL;
        break;
      case WL:
        chooser[g_pc_lower_bits] = (outcome==(bht_gshare[index]>>1))?WG:SL;
        break;
      case WG:
        chooser[g_pc_lower_bits] = (outcome==(bht_gshare[index]>>1))?SG:WL;
        break;
      case SG:
        chooser[g_pc_lower_bits] = (outcome==(bht_gshare[index]>>1))?SG:WG;
        break;
      default:
        printf("Warning: Undefined state of entry in CHOOSER! (TOUR)\n");
    }
  }
  


  //local
  switch(bht_local[pht_local[l_pc_lower_bits]]){
    case WN:
      bht_local[pht_local[l_pc_lower_bits]] = (outcome==TAKEN)?WT:SN;
      break;
    case SN:
      bht_local[pht_local[l_pc_lower_bits]] = (outcome==TAKEN)?WN:SN;
      break;
    case WT:
      bht_local[pht_local[l_pc_lower_bits]] = (outcome==TAKEN)?ST:WN;
      break;
    case ST:
      bht_local[pht_local[l_pc_lower_bits]] = (outcome==TAKEN)?ST:WT;
      break;
    default:
      printf("Warning: Undefined state of entry in LOCAL BHT! (TOUR)\n");
  }
  //update PHT
  pht_local[l_pc_lower_bits] = ((pht_local[l_pc_lower_bits]<< 1) | outcome); 

  //gshare
  //Update state of entry in bht based on outcome
  switch(bht_gshare[index]){
    case WN:
      bht_gshare[index] = (outcome==TAKEN)?WT:SN;
      break;
    case SN:
      bht_gshare[index] = (outcome==TAKEN)?WN:SN;
      break;
    case WT:
      bht_gshare[index] = (outcome==TAKEN)?ST:WN;
      break;
    case ST:
      bht_gshare[index] = (outcome==TAKEN)?ST:WT;
      break;
    default:
      printf("Warning: Undefined state of entry in GSHARE BHT! (TOUR)\n");
  }

  //Update history register
  ghistory = ((ghistory << 1) | outcome); 

  
  
}

void
cleanup_tournament() {
  free(bht_gshare);
  free(pht_local);
  free(bht_local);
  free(chooser);
}

//start_custom
void init_custom() {
  //perceptron table
  int perc_entries = 1 << perc_ghistoryBits;
  perc_table = (int**)malloc(perc_entries * sizeof(int*));
  
  //initiallize all the weights to 111..111
  int i;
  for(i = 0; i< perc_entries; i++){
    int j;
    perc_table[i] = (int*)malloc((perc_ghistoryBits+1) * sizeof(int));
    for(j = 0; j < perc_ghistoryBits+1; j++){
      perc_table[i][j] = 1;
    }
  }
  // printf("here %d\n", i);
  perc_ghistory = 0;
}



uint8_t 
custom_predict(uint32_t pc) {
  uint32_t perc_w_entries = 1 << perc_ghistoryBits;//2^12
  uint32_t pc_lower_bits = pc & (perc_w_entries-1);//TODO: why -1?
  // uint32_t perc_g_entries = 1 << perc_ghistoryBits;
  uint32_t ghistory_lower_bits = ghistory & (perc_w_entries-1);
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;
  int i;
  int result = 0;
  result += perc_table[index][0] * 1; //balance
  for(i=1; i<perc_ghistoryBits+1; i++){
    if (1 & (ghistory_lower_bits >> i)){
      result += perc_table[index][i] * 1;
    }else{
      result += perc_table[index][i] * -1;
    }
    // result += (1 & (ghistory_lower_bits >> i))?perc_table*[pc_lower_bits][i] * 1:perc_table[pc_lower_bits][i] * -1;
  }
  return (result >= 0)?TAKEN:NOTTAKEN;
}

void
train_custom(uint32_t pc, uint8_t outcome) {
  uint32_t perc_w_entries = 1 << perc_ghistoryBits;
  uint32_t pc_lower_bits = pc & (perc_w_entries-1);
  // uint32_t perc_g_entries = 1 << perc_ghistoryBits;
  uint32_t ghistory_lower_bits = ghistory & (perc_w_entries-1);
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;
  int i;
  int result = 0;
  result += perc_table[index][0] * 1; //balance
  for(i=1; i<perc_ghistoryBits+1; i++){
    if (1 & (ghistory_lower_bits >> i)){
      result += perc_table[index][i] * 1;
    }else{
      result += perc_table[index][i] * -1;
    }
  }
  int b_output = (result >= 0)?TAKEN:NOTTAKEN;
  int outcome_pos_neg = (outcome)?1:-1;
  
  if(outcome != b_output || abs(result) < thred){
    perc_table[index][0] += 50 * outcome_pos_neg; //balance
    for(i=1; i<perc_ghistoryBits+1; i++){
      if (1 & (ghistory_lower_bits >> i)){
          if(perc_table[index][i] > -8 && perc_table[index][i] < 7){
            perc_table[index][i] += 1 * outcome_pos_neg;
          } 
      }else{
          if(perc_table[index][i] > -8 && perc_table[index][i] < 7){
            perc_table[index][i] += -1 * outcome_pos_neg;
          }
          
      }
    }
  }
  
  //Update history register
  ghistory = ((ghistory << 1) | outcome); 
}

void
cleanup_custom() {
  int perc_entries = 1 << perc_ghistoryBits;
  int i;
  for(i = 0; i< perc_entries; i++){
    free(perc_table[i]);
  }
  free(perc_table);
}

void
init_predictor()
{
  switch (bpType) {
    case STATIC:
    case GSHARE:
      init_gshare();
      break;
    case TOURNAMENT:
      init_tournament();
      break;
    case CUSTOM:
      init_custom();
      break;
    default:
      break;
  }
  
}

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
uint8_t
make_prediction(uint32_t pc)
{

  // Make a prediction based on the bpType
  switch (bpType) {
    case STATIC:
      return TAKEN;
    case GSHARE:
      return gshare_predict(pc);
    case TOURNAMENT:
      return tournament_predict(pc);
    case CUSTOM:
      return custom_predict(pc);
    default:
      break;
  }

  // If there is not a compatable bpType then return NOTTAKEN
  return NOTTAKEN;
}

// Train the predictor the last executed branch at PC 'pc' and with
// outcome 'outcome' (true indicates that the branch was taken, false
// indicates that the branch was not taken)
//

void
train_predictor(uint32_t pc, uint8_t outcome)
{

  switch (bpType) {
    case STATIC:
    case GSHARE:
      return train_gshare(pc, outcome);
    case TOURNAMENT:
      return train_tournament(pc, outcome);
    case CUSTOM:
      return train_custom(pc, outcome);
    default:
      break;
  }
  

}
