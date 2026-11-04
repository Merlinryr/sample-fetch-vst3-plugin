#include "Processor.h"
#include "Editor.h"

juce::AudioProcessorEditor* Processor::createEditor() { return new Editor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Processor(); }
