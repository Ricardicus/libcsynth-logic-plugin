#include "Parameters.h"
#include "EchoTiming.h"
#include <cmath>

namespace parameters {
juce::String layerId(int layer, const juce::String& field) { return "l" + juce::String(layer) + "_" + field; }
juce::String operatorId(int layer, int op, const juce::String& field) { return layerId(layer, "o" + juce::String(op) + "_" + field); }
double Spec::read(const SynthConfig& c) const
{
    auto* field = reinterpret_cast<const char*>(&c) + offset;
    switch (kind) {
        case Kind::real: return *reinterpret_cast<const double*>(field);
        case Kind::integer: return *reinterpret_cast<const int*>(field);
        case Kind::waveform: return *reinterpret_cast<const Waveform*>(field);
        case Kind::algorithm: return *reinterpret_cast<const FmAlgorithm*>(field);
        case Kind::indexMode: return *reinterpret_cast<const FmIndexMode*>(field);
    }
    return 0;
}
void Spec::write(SynthConfig& c, float value) const
{
    if (!std::isfinite(value)) value = minimum;
    value = juce::jlimit(minimum, maximum, value);
    // Cutoff zero bypasses. Intermediate host values below 20 Hz also bypass.
    if ((id == "lowpass" || id == "highpass") && value < 20) value = 0;
    auto* field = reinterpret_cast<char*>(&c) + offset;
    switch (kind) {
        case Kind::real: *reinterpret_cast<double*>(field) = value; break;
        case Kind::integer: *reinterpret_cast<int*>(field) = juce::roundToInt(value); break;
        case Kind::waveform: *reinterpret_cast<Waveform*>(field) = static_cast<Waveform>(juce::roundToInt(value)); break;
        case Kind::algorithm: *reinterpret_cast<FmAlgorithm*>(field) = static_cast<FmAlgorithm>(juce::roundToInt(value)); break;
        case Kind::indexMode: *reinterpret_cast<FmIndexMode*>(field) = static_cast<FmIndexMode>(juce::roundToInt(value)); break;
    }
}
const std::vector<Spec>& specs()
{
    static const auto all = [] {
        std::vector<Spec> v;
        auto add = [&](juce::String id, juce::String name, std::size_t offset, Kind kind,
                       float low, float high, float step = .01f, float skew = 1) {
            v.push_back({id,name,offset,kind,low,high,step,skew});
        };
        auto env = offsetof(SynthConfig,outputEnvelope);
        add("attack","Master attack",env+offsetof(SynthEnvelopeConfig,attackMs),Kind::integer,0,10000,1,.3f);
        add("decay","Master decay",env+offsetof(SynthEnvelopeConfig,decayMs),Kind::integer,0,10000,1,.3f);
        add("sustain","Master sustain",env+offsetof(SynthEnvelopeConfig,sustainPercent),Kind::integer,0,100,1);
        add("release","Master release",env+offsetof(SynthEnvelopeConfig,releaseMs),Kind::integer,0,10000,1,.3f);
        auto filters = offsetof(SynthConfig,filters);
        add("lowpass","Low-pass cutoff",filters+offsetof(SynthFilterConfig,lowpassHz),Kind::real,0,20000,1,.25f);
        add("highpass","High-pass cutoff",filters+offsetof(SynthFilterConfig,highpassHz),Kind::real,0,20000,1,.25f);
        auto fx = offsetof(SynthConfig,effects);
        add("echoMix","Echo mix",fx+offsetof(SynthEffectsConfig,echoMix),Kind::real,0,1);
        add("echoDelay","Echo delay",fx+offsetof(SynthEffectsConfig,echoDelayMs),Kind::real,1,2000,1,.5f);
        add("echoFeedback","Echo feedback",fx+offsetof(SynthEffectsConfig,echoFeedback),Kind::real,0,.95f);
        add("reverbMix","Reverb mix",fx+offsetof(SynthEffectsConfig,reverbMix),Kind::real,0,1);
        add("reverbRoom","Reverb room",fx+offsetof(SynthEffectsConfig,reverbRoom),Kind::real,0,.95f);
        add("reverbDamping","Reverb damping",fx+offsetof(SynthEffectsConfig,reverbDamping),Kind::real,0,1);
        add("layers","Layers",offsetof(SynthConfig,layerCount),Kind::integer,1,8,1);
        for (int l=0;l<SYNTH_MAX_LAYERS;++l) {
            auto base = offsetof(SynthConfig,layers) + static_cast<std::size_t>(l)*sizeof(SynthLayerConfig);
            auto prefix = "Layer " + juce::String(l+1) + " ";
            add(layerId(l,"gain"),prefix+"gain",base+offsetof(SynthLayerConfig,gain),Kind::real,0,1);
            add(layerId(l,"detune"),prefix+"detune",base+offsetof(SynthLayerConfig,detuneCents),Kind::real,-4800,4800,1);
            auto fm = base + offsetof(SynthLayerConfig,fm);
            add(layerId(l,"count"),prefix+"operators",fm+offsetof(FmConfig,operatorCount),Kind::integer,1,8,1);
            for (int o=0;o<FM_MAX_OPERATORS;++o) {
                auto op = fm + offsetof(FmConfig,operators) + static_cast<std::size_t>(o)*sizeof(FmOperatorConfig);
                auto name = prefix + "OP" + juce::String(o+1) + " ";
                auto addOp = [&](const char* field, const char* label, std::size_t offset, Kind kind,
                                 float low, float high, float step=.01f, float skew=1) {
                    add(operatorId(l,o,field),name+label,op+offset,kind,low,high,step,skew);
                };
                addOp("wave","waveform",offsetof(FmOperatorConfig,waveform),Kind::waveform,0,WAVE_COUNT-1,1);
                addOp("ratio","ratio",offsetof(FmOperatorConfig,ratio),Kind::real,.01f,32,.01f,.5f);
                addOp("depth","FM depth",offsetof(FmOperatorConfig,rm),Kind::real,0,32,.01f,.5f);
                addOp("pulse","pulse width",offsetof(FmOperatorConfig,pulseWidth),Kind::real,.01f,.99f,.001f);
                addOp("vibRate","vibrato rate",offsetof(FmOperatorConfig,vibratoRateHz),Kind::real,0,50,.01f,.5f);
                addOp("vibDepth","vibrato depth",offsetof(FmOperatorConfig,vibratoDepthCents),Kind::real,0,1200,.1f,.5f);
                addOp("mode","index envelope",offsetof(FmOperatorConfig,indexMode),Kind::indexMode,0,2,1);
                addOp("expDecay","index decay rate",offsetof(FmOperatorConfig,decayRate),Kind::real,0,100,.01f,.5f);
                addOp("attack","index attack",offsetof(FmOperatorConfig,attackMs),Kind::integer,0,10000,1,.3f);
                addOp("decay","index decay",offsetof(FmOperatorConfig,decayMs),Kind::integer,0,10000,1,.3f);
                addOp("sustain","index sustain",offsetof(FmOperatorConfig,sustainPercent),Kind::integer,0,100,1);
                addOp("release","index release",offsetof(FmOperatorConfig,releaseMs),Kind::integer,0,10000,1,.3f);
            }
        }
        // Append new specs so existing parameter ordering remains stable.
        for (int l=0;l<SYNTH_MAX_LAYERS;++l) {
            auto fm=offsetof(SynthConfig,layers)+static_cast<std::size_t>(l)*sizeof(SynthLayerConfig)+offsetof(SynthLayerConfig,fm);
            auto prefix="Layer "+juce::String(l+1)+" ";
            add(layerId(l,"algorithm"),prefix+"FM algorithm",fm+offsetof(FmConfig,algorithm),Kind::algorithm,0,FM_ALGORITHM_COUNT-1,1);
            for (int o=0;o<FM_MAX_OPERATORS;++o) {
                auto offset=fm+offsetof(FmConfig,operators)+static_cast<std::size_t>(o)*sizeof(FmOperatorConfig);
                add(operatorId(l,o,"output"),prefix+"OP"+juce::String(o+1)+" output level",offset+offsetof(FmOperatorConfig,outputLevel),Kind::real,0,1,.001f);
                add(operatorId(l,o,"feedback"),prefix+"OP"+juce::String(o+1)+" feedback",offset+offsetof(FmOperatorConfig,feedback),Kind::real,0,8,.001f,.5f);
                for (int d=o+1;d<FM_MAX_OPERATORS;++d)
                    add(layerId(l,"route_"+juce::String(o)+"_"+juce::String(d)),prefix+"OP"+juce::String(o+1)+" to OP"+juce::String(d+1),
                        fm+offsetof(FmConfig,routing)+(static_cast<std::size_t>(o)*FM_MAX_OPERATORS+static_cast<std::size_t>(d))*sizeof(double),Kind::real,0,1,.001f);
            }
        }
        return v;
    }();
    return all;
}
bool routingParameter(const Spec& s)
{
    return s.kind==Kind::algorithm || s.id.endsWith("_output") || s.id.endsWith("_feedback") || s.id.contains("_route_");
}
juce::AudioProcessorValueTreeState::ParameterLayout layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout result;
    auto defaults = synthPresetConfig(0);
    auto append=[&](const Spec& s) {
        juce::ParameterID id{s.id,1};
        if (s.kind == Kind::algorithm) {
            juce::StringArray choices;
            for (int i=0;i<FM_ALGORITHM_COUNT;++i) choices.add(fmAlgorithmName(static_cast<FmAlgorithm>(i)));
            result.add(std::make_unique<juce::AudioParameterChoice>(id,s.name,choices,static_cast<int>(s.read(defaults))));
        } else if (s.kind == Kind::waveform) {
            result.add(std::make_unique<juce::AudioParameterChoice>(id,s.name,
                juce::StringArray{"Sine","Square","Triangle","Saw","Pulse","Noise"},static_cast<int>(s.read(defaults))));
        } else if (s.kind == Kind::indexMode) {
            result.add(std::make_unique<juce::AudioParameterChoice>(id,s.name,
                juce::StringArray{"Sustain","Decay","ADSR"},static_cast<int>(s.read(defaults))));
        } else {
            juce::NormalisableRange<float> range{s.minimum,s.maximum,s.interval,s.skew};
            result.add(std::make_unique<juce::AudioParameterFloat>(id,s.name,range,static_cast<float>(s.read(defaults))));
        }
    };
    for (const auto& s : specs()) if (!routingParameter(s)) append(s);
    result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"gain",1},"Output gain",
        juce::NormalisableRange<float>{-24,12,.1f},0));
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"echoTiming",1},"Echo timing",echoTiming::choices(),0));
    for (const auto& s : specs()) if (routingParameter(s)) append(s);
    return result;
}
}
