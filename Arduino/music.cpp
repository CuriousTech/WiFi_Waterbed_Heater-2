#include "music.h"
//  More songs available at https://github.com/robsoncouto/arduino-songs
//                                             Robson Couto, 2019

// notes of the moledy followed by the duration.
// a 4 means a quarter note, 8 an eighteenth , 16 sixteenth, so on
// !!negative numbers are used to represent dotted notes,
// so -4 means a dotted quarter note, that is, a quarter plus an eighteenth!!
const int melody[] = {

  // Pacman
  // Score available at https://musescore.com/user/85429/scores/107109
  NOTE_B4, 16, NOTE_B5, 16, NOTE_FS5, 16, NOTE_DS5, 16, //1
  NOTE_B5, 32, NOTE_FS5, -16, NOTE_DS5, 8, NOTE_C5, 16,
  NOTE_C6, 16, NOTE_G6, 16, NOTE_E6, 16, NOTE_C6, 32, NOTE_G6, -16, NOTE_E6, 8,

  NOTE_B4, 16,  NOTE_B5, 16,  NOTE_FS5, 16,   NOTE_DS5, 16,  NOTE_B5, 32,  //2
  NOTE_FS5, -16, NOTE_DS5, 8,  NOTE_DS5, 32, NOTE_E5, 32,  NOTE_F5, 32,
  NOTE_F5, 32,  NOTE_FS5, 32,  NOTE_G5, 32,  NOTE_G5, 32, NOTE_GS5, 32,  NOTE_A5, 16, NOTE_B5, 8
};

bool Music::add(uint16_t note, uint16_t delay)
{
  if(m_bPlaying)
  {
    if(m_idx >= MUS_LEN)
      return false;
    m_arr[m_idx].note = note;
    m_arr[m_idx].ms = delay;
    m_idx++;
  }
  else
  {
    analogWriteFreq(note);
    analogWrite(TONE, 500);
    m_bPlaying = true;
    m_toneEnd = millis() + delay;
  }
  return true;
}

bool Music::play(int song)
{
  int tempo = 100;
  int notes;
  const int *pSong;

  switch(song)
  {
    case 0:
      tempo = 105;
      notes = sizeof(melody) / sizeof(melody[0]) / 2;
      pSong = melody;
      break;
  }
  if(!pSong || !notes)
    return false;
  if(notes >= MUS_LEN - m_idx)
    notes = MUS_LEN - m_idx;

  // this calculates the duration of a whole note in ms
  int wholenote = (60000 * 4) / tempo;
  int divider = 0, noteDuration = 0;

  for (int thisNote = 0; thisNote < notes * 2; thisNote = thisNote + 2)
  {
    // calculates the duration of each note
    divider = pSong[thisNote + 1];
    if (divider > 0) {
      // regular note, just proceed
      noteDuration = (wholenote) / divider;
    } else if (divider < 0) {
      // dotted notes are represented with negative durations!!
      noteDuration = (wholenote) / abs(divider);
      noteDuration *= 1.5; // increases the duration in half for dotted notes
    }

    // we only play the note for 90% of the duration, leaving 10% as a pause
    add(pSong[thisNote], noteDuration * 0.9);
  }
  return true;
}

void Music::service()
{
  if(m_bPlaying == false)
    return;
  else if(millis() >= m_toneEnd)
  {
    analogWrite(TONE, 0);
    m_toneEnd = 0;
    m_bPlaying = false;
  }
  else
    return;
  if(m_idx == 0)
    return;
  analogWriteFreq(m_arr[0].note);
  analogWrite(TONE, 500);
  m_toneEnd = millis() + m_arr[0].ms;
  memcpy(m_arr, m_arr + 1, sizeof(musicArr) * MUS_LEN);
  m_idx--;
  m_bPlaying = true;
}
