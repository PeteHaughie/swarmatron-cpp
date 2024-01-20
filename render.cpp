/*
 ____  _____ _        _
| __ )| ____| |      / \
|  _ \|  _| | |     / _ \
| |_) | |___| |___ / ___ \
|____/|_____|_____/_/   \_\
http://bela.io
*/
/**

Swarmatron
---------------------------

This sketch is an interpetation of the Dewanatron Swarmatron by Pete Haughie.

It produces a square wave for each voice which follows its sine.

If the frequency of the note drops below 20hz then it is reduced to 0 to prevent it from introducing rather unpleasant clicks when the spread between voices is low.

It uses two Trill Bar sensors but could potentially use other types of Trill sensors if you so desired. I could imagine a version which uses a single touch on a Square xy + touch size.

Please feel free to build your own and use this sketch as your basis.

*/

#include <Bela.h>
#include <cmath>
#include <libraries/Trill/Trill.h>
#include <libraries/Scope/Scope.h>

std::vector<Trill*> gTouchSensors;

#define NUM_VOICES 8

Scope scope;
Trill touchSensor;
unsigned int gSampleCount = 0;

// Sleep time for auxiliary task
unsigned int gTaskSleepTime = 1200; // microseconds - this could be slower if you wanted to save CPU

float gScaler = 1.0 / (float)NUM_VOICES;

float gInverseSampleRate;

float gMultipliers[] = { 0.0, 0.8, 0.75, 0.5, 0.25, 0.33, 0.66, 0.8 };
float gFreqs[NUM_VOICES] = { 0.0 };
float gOuts[NUM_VOICES] = { 0.0 };
float gPhases[NUM_VOICES] = { 0.0 };
float gSquares[NUM_VOICES] = { 0.0 };

float gSpread = 0.0;
float gVoiceAmp = 0.0;

int gCounter = 0;

// sensor states
// 0 = Trill 1 waiting
// 1 = Trill 1 active
int gTouch = 0;

// play states
// 0 = at rest
// 1 = ramp up
// 2 = playing
// 3 = ramp down
int gAudio = 0;
int gRamp = 0;
float gRampMax = 10000.0; // 5ms ramp - recaulculate this with potentiometer if neccessary
float gRampMultiplier = 0.0;
float gRampIncrementor = 1.0 / gRampMax;

/*
* Function to be run on an auxiliary task that reads data from the Trill sensor.
* Here, a loop is defined so that the task runs recurrently for as long as the
* audio thread is running.
*/
void loop(void*)
{
  while(!Bela_stopRequested())
  {
    for(unsigned int n = 0; n < gTouchSensors.size(); ++n)
    {
      Trill* t = gTouchSensors[n];
      t->readI2C();
    }
    usleep(gTaskSleepTime);
  }
}


bool setup(BelaContext *context, void *userData)
{
  rt_printf("gRampIncrementor: %f\n", gRampIncrementor);
  unsigned int i2cBus = 1;
  for(uint8_t addr = 0x20; addr <= 0x50; ++addr)
  {
    Trill::Device device = Trill::probe(i2cBus, addr);
    if(Trill::NONE != device && Trill::CRAFT != device)
    {
      gTouchSensors.push_back(new Trill(i2cBus, device, addr));
      gTouchSensors.back()->printDetails();
    }
  }
  
  // Set and schedule auxiliary task for reading sensor data from the I2C bus
  Bela_runAuxiliaryTask(loop);
  
  gInverseSampleRate = 1.0 / context->audioSampleRate;
  scope.setup(7, context->audioSampleRate);
  return true;
}

void render(BelaContext *context, void *userData)
{
  for(unsigned int n = 0; n < context->audioFrames; n++) {
    // gCounter++;
    float out = 0.0;
    gSampleCount++;
    if(gSampleCount == gTaskSleepTime)
    {
      gSampleCount = 0;
      float arr[3];
      for(unsigned int t = 0; t < gTouchSensors.size(); ++t) {
        arr[0] = gTouchSensors[t]->compoundTouchSize();
        arr[1] = gTouchSensors[t]->compoundTouchLocation();
        arr[2] = gTouchSensors[t]->compoundTouchHorizontalLocation(); // this is only for the Trill Square
        if (t == 0) { // this is Bar #1
          // You're probably wondering quite rightly what this is doing. The Trill returns a value of 0 - 1 and we're going to be doing floating point calculations on the value.
          // If values are < 1 then the maths gets weird quickly so this way we keep it > 1.
          // Adding 111 keeps the hertz in audible range but very low.
          // eg:
          // 0.5 + 1 = 1.5
          // 1.5 * 100 = 150
          // 150 / 2 = 75 (I'm not sure why I do this step to be honest but it was there in my Pure Data version so here it stays to keep the sound the same)
          // 75 + 111 = 186hz
          if (arr[0] > 0.1) { // is the bar being touched?
        	gFreqs[0] = ((arr[1] + 1) * 100) / 2 + 111; // set the fundamental frequency
	        gVoiceAmp = constrain(arr[0], 0.0, 1.0);
        	if (gTouch == 0) {
	        	gTouch = 1;
	        	gAudio = 1;
	        	rt_printf("touch moving to 1\n");
	        	rt_printf("audio moving to 1\n");
        	}
          } else {
          	if (gTouch == 1) {
	          	rt_printf("touch moving to 0\n");
	          	rt_printf("audio moving to 3\n");
	          	gTouch = 0;
	          	gAudio = 3;
          	}
          }
          // TODO: touch size controls amplitude of each voice - the closer to 1 the louder it is but constrained to 0.8
          // Work out whether it's better to alter the gScaler (unlikely) or have a modifier for the voices (likely)
          // gScaler = arr[0] > 0.1 ? map(arr[0], 0.0, 1.0, 0.0, 0.8) : 0.0;
          //rt_printf("touchsize: %f\n", gVoiceAmp);
        }
        if (t == 1) { // this is the Bar #2
          gSpread = arr[0] > 0.1 ? arr[1] : 0.0;
        }
      }
      

    }
    for (int i = 0; i < NUM_VOICES; i++) { 
          // Get the frequency of this oscillator (if it's the first one we multiply it by 1 and it doesn't change):
          if (i > 0) {
            float var = i < 5 ? gMultipliers[i] / gSpread + 1 : gMultipliers[i] * gSpread + 1;
        	gFreqs[i] = gFreqs[0] / (var) < 20 ? 0.0 : gFreqs[0] / (var);
          /*
          // grab a few values from the first voice to see what's going on
          if (i == 1) {
            if (gCounter % 10000 == 0) {
              rt_printf("var: %f\n", var);
              rt_printf("fundamental: %f\n", gFreqs[0]);
              rt_printf("freq: %f\n", gFreqs[i]);
              rt_printf("spread: %f\n", gSpread);
              gCounter = 0;
            }
          }
          */
          } 
      // Calculate the output:
      gOuts[i] = 0.8f * sinf(gPhases[i]);
      //gOuts[i] = gVoiceAmp * sinf(gPhases[i]);
      if(gPhases[i] > M_PI) {
        gPhases[i] -= 2.0f * (float)M_PI;
      }
      // Update the phase based on the freq we saved:
      gPhases[i] += 2.0f * (float)M_PI * gFreqs[i] * gInverseSampleRate;
      // Collect all outputs in our collector number, and make sure they're scaled:
      if (gOuts[i] > 0) {
        gSquares[i] = 1.0 * gScaler;
      } else {
        gSquares[i] = -1.0 * gScaler;
      }
      out += gSquares[i];
    }
    // uncomment me to see the square waves on the scope:
    // scope.log(gSquares[0], gSquares[1], gSquares[2], gSquares[3], gSquares[4], gSquares[5], gSquares[6], gSquares[7]);
    out *= gVoiceAmp;
    // audio states
    
    /*
    int gAudio = 0;
	int gRamp = 0;
	int gRampMax = 220; // 5ms ramp - recaulculate this with potentiometer if neccessary
	float gRampMultiplier = 0.0;
	float gRampIncrementor = 1 / gRampMax;
    */
	if (gAudio == 0) {
		gRampMultiplier = 0;
	} else if (gAudio == 1) {
		gCounter++;
		gRampMultiplier += gRampIncrementor;
		if (gCounter % ((int)gRampMax / 10) == 0) {
			rt_printf("gRampMultiplier %f\n",  gRampMultiplier);
		}
		if (gRampMultiplier >= 1.0) {
			gRampMultiplier = 1.0;
          	rt_printf("audio moving to 2\n");
			gAudio = 2;
		}
	} else if (gAudio == 2) {
		gRampMultiplier = 1.0;
	} else if (gAudio == 3) {
		gCounter++;
		gRampMultiplier -= gRampIncrementor;
		if (gCounter % ((int)gRampMax / 10) == 0) {
			rt_printf("gRampMultiplier %f\n",  gRampMultiplier);
		}
		if (gRampMultiplier <= 0.0) {
			gRampMultiplier = 0.0;
          	rt_printf("audio moving to 0\n");
			gAudio = 0;
		}
	}
	out *= gRampMultiplier;
    for(unsigned int channel = 0; channel < context->audioOutChannels; channel++) {
      audioWrite(context, n, channel, out);
    }
  }
}

void cleanup(BelaContext *context, void *userData)
{
  for(auto t : gTouchSensors)
    delete t;
}
