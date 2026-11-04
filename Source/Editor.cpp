#include "Editor.h"

using namespace juce;

Editor::Editor (Processor& p) : AudioProcessorEditor (p), fetcher (p.fetcher)
{
    url.setTextToShowWhenEmpty ("Paste a YouTube link", Colours::grey);
    url.setTitle ("YouTube link");
    url.onReturnKey = [this] { download.triggerClick(); };

    download.onClick = [this] { fetcher.fetch (url.getText()); };
    update.onClick = [this] { fetcher.updateTools(); };
    openFolder.onClick = [this]
    {
        const auto file = fetcher.snapshot().result;
        if (file.existsAsFile())
            file.revealToUser();
        else if (Fetcher::outputDir().createDirectory())
            Fetcher::outputDir().startAsProcess();
    };

    name.setTitle ("File name");
    name.setTextToShowWhenEmpty ("File name", Colours::grey);
    name.onReturnKey = [this] { applyName(); };
    name.onFocusLost = [this] { applyName(); };

    send.setTooltip ("Click: add to Live's browser (User Library > Samples > SampleFetch).\nDrag: drop straight onto a track.");
    send.onClick = [this]
    {
        applyName();
        if (! dragged)
            fetcher.sendToLive();
    };
    send.addMouseListener (this, false);

    for (auto* l : { &bpmLabel, &keyLabel })
    {
        l->setFont (FontOptions (24.0f, Font::bold));
        l->setJustificationType (Justification::centred);
        l->setColour (Label::textColourId, Colours::grey);
    }
    bpmLabel.setText ("-- BPM", dontSendNotification);
    keyLabel.setText ("-- key", dontSendNotification);
    bpmLabel.setTitle ("Tempo");
    keyLabel.setTitle ("Key");

    for (auto* c : std::initializer_list<Component*> { &url, &download, &bar, &status, &bpmLabel, &keyLabel, &name, &send, &openFolder, &update })
        addAndMakeVisible (c);

    setSize (520, 250);
    timerCallback();
    startTimerHz (20);
}

void Editor::paint (Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
}

void Editor::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto row = [&r] (int h) { auto s = r.removeFromTop (h); r.removeFromTop (8); return s; };

    auto top = row (28);
    download.setBounds (top.removeFromRight (100));
    url.setBounds (top.withTrimmedRight (8));

    bar.setBounds (row (20));
    status.setBounds (row (20));

    auto info = row (42);
    bpmLabel.setBounds (info.removeFromLeft (info.getWidth() / 2).withTrimmedRight (4));
    keyLabel.setBounds (info.withTrimmedLeft (4));

    auto file = row (36);
    send.setBounds (file.removeFromRight (120));
    name.setBounds (file.withTrimmedRight (8).withSizeKeepingCentre (file.getWidth() - 8, 28));

    auto bottom = row (24);
    openFolder.setBounds (bottom.removeFromLeft (110));
    update.setBounds (bottom.removeFromRight (110));
}

void Editor::timerCallback()
{
    const auto s = fetcher.snapshot();
    const bool busy = fetcher.isBusy();

    progress = s.progress;
    status.setText (s.status, dontSendNotification);
    download.setEnabled (! busy);
    update.setEnabled (! busy);

    if (s.result != shown)
    {
        shown = s.result;
        name.setText (shown.getFileName(), false);

        const auto& a = s.analysis;
        bpmLabel.setText (a.bpm > 0 ? String (a.bpm) + " BPM" : "-- BPM", dontSendNotification);
        keyLabel.setText (a.key.isNotEmpty() ? a.camelot + "  (" + a.key + ")" : "-- key", dontSendNotification);
        // badge colour follows the Camelot wheel, so compatible keys (neighbouring numbers) look alike
        const auto hue = a.key.isNotEmpty() ? (float) (a.camelot.getIntValue() - 1) / 12.0f : 0.0f;
        keyLabel.setColour (Label::backgroundColourId, a.key.isNotEmpty() ? Colour::fromHSV (hue, 0.5f, 0.85f, 1.0f) : Colours::transparentBlack);
        keyLabel.setColour (Label::textColourId, a.key.isNotEmpty() ? Colours::black : Colours::grey);
        bpmLabel.setColour (Label::textColourId, a.bpm > 0 ? Colours::white : Colours::grey);
    }
    const bool ready = shown.existsAsFile() && ! busy;
    name.setEnabled (ready);
    send.setEnabled (ready);
}

void Editor::applyName()
{
    if (name.getText() != shown.getFileName() && ! fetcher.renameResult (name.getText()))
        name.setText (shown.getFileName(), false); // invalid or taken name: revert
}

void Editor::mouseDown (const MouseEvent& e)
{
    if (e.eventComponent == &send)
        dragged = false;
}

void Editor::mouseDrag (const MouseEvent& e)
{
    if (e.eventComponent != &send || ! send.isEnabled() || dragged || e.getDistanceFromDragStart() < 6)
        return;

    dragged = true;
    applyName();
    const auto file = fetcher.snapshot().result;
    if (file.existsAsFile())
        DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
}
