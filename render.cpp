/*
 ____  _____ _        _
| __ )| ____| |      / \
|  _ \|  _| | |     / _ \
| |_) | |___| |___ / ___ \
|____/|_____|_____/_/   \_\
http://bela.io
*/
/**
\example Fundamentals/sinetone/render.cpp

Producing your first bleep!
---------------------------

This sketch is the hello world of embedded interactive audio. Better known as bleep, it
produces a sine tone.

The frequency of the sine tone is determined by a global variable, `gFrequency`.
The sine tone is produced by incrementing the phase of a sin function
on every audio frame.

In render() you'll see a nested for loop structure. You'll see this in all Bela projects.
The first for loop cycles through 'audioFrames', the second through 'audioChannels' (in this case left 0 and right 1).
It is good to familiarise yourself with this structure as it's fundamental to producing sound with the system.
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
unsigned int gTaskSleepTime = 600; // microseconds

float gScaler = 1.0 / (double)NUM_VOICES;

float gInverseSampleRate;

float gMultipliers[] = { 0.0, 0.8, 0.75, 0.5, 0.25, 0.33, 0.66, 0.8 };
float gFreqs[NUM_VOICES] = { 0.0 };
float gOuts[NUM_VOICES] = { 0.0 };
float gPhases[NUM_VOICES] = { 0.0 };
float gSquares[NUM_VOICES] = { 0.0 };

float gOut;
float gSpread = 0.0;

int counter = 0;

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
		counter++;
		gOut = 0.0;
		// TO DO: Get fundamental frequency from Trill Bar position:
		gSampleCount++;
		if(gSampleCount == gTaskSleepTime)
		{
			gSampleCount = 0;
			float arr[3];
			for(unsigned int t = 0; t < gTouchSensors.size(); ++t) {
				arr[0] = gTouchSensors[t]->compoundTouchSize();
				arr[1] = gTouchSensors[t]->compoundTouchLocation();
				arr[2] = gTouchSensors[t]->compoundTouchHorizontalLocation();
				if (t == 0) { // this is our bar I think
					gFreqs[0] = arr[0] > 0.1 ? ((arr[1] + 1) * 100) / 2 + 111 : 0.0; // if the touch size is larger than nothing then set the frequency to the position and map it.
				}
				if (t == 1) { // this is the ring - eventually to be a bar
					gSpread = arr[0] > 0.1 ? arr[1] : 0.0;
					// rt_printf("%f", gSpread);
				}
				// rt_printf("[%d] %2.3f %.3f %.3f  ", t, arr[0], arr[1], arr[2]);
			}
			// rt_printf("\n");

		}
		for (int i = 0; i < NUM_VOICES; i++) { 
        	// Get the frequency of this oscillator (if it's the first one we multiply it by 1 and it doesn't change):
        	if (i > 0) {
        		float var = i < 5 ? gMultipliers[i] / gSpread + 1 : gMultipliers[i] * gSpread + 1;
	 	 		gFreqs[i] = gFreqs[0] / (var) < 20 ? 0.0 : gFreqs[0] / (var);
	 	 		/*
				if (i == 1) {
					if (counter % 10000 == 0) {
						rt_printf("var: %f\n", var);
						rt_printf("fundamental: %f\n", gFreqs[0]);
		    			rt_printf("freq: %f\n", gFreqs[i]);
		    			rt_printf("spread: %f\n", gSpread);
		    			counter = 0;
					}
	        	}
	        	*/
        	} 
			// Calculate the output:
			gOuts[i] = 0.8f * sinf(gPhases[i]);
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
			gOut += gSquares[i];
		}
		
		// scope.log(gSquares[0], gSquares[1], gSquares[2], gSquares[3], gSquares[4], gSquares[5], gSquares[6], gSquares[7]);
		for(unsigned int channel = 0; channel < context->audioOutChannels; channel++) {
			audioWrite(context, n, channel, gOut);
		}
	}
}

void cleanup(BelaContext *context, void *userData)
{
	for(auto t : gTouchSensors)
		delete t;
}
