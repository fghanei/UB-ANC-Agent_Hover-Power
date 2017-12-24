#include <QSettings>
#include <QDebug>
#include <QCoreApplication>
#include <QFile>
#include <QRegularExpression>

#include "QGC.h"
#include "QGCAudioWorker.h"
#include "GAudioOutput.h"

#if (defined __macos__) && defined QGC_SPEECH_ENABLED
#include <ApplicationServices/ApplicationServices.h>
#include <MacTypes.h>

void macSpeak(const char* words)
{
    static SpeechChannel sc = NULL;

    while (SpeechBusy()) {
        QGC::SLEEP::msleep(100);
    }
    if (sc == NULL) {
        Float32 volume = 1.0;

        NewSpeechChannel(NULL, &sc);
        CFNumberRef volumeRef = CFNumberCreate(NULL, kCFNumberFloat32Type, &volume);
        SetSpeechProperty(sc, kSpeechVolumeProperty, volumeRef);
        CFRelease(volumeRef);
    }
    CFStringRef strRef = CFStringCreateWithCString(NULL, words, kCFStringEncodingUTF8);
    SpeakCFString(sc, strRef, NULL);
    CFRelease(strRef);
}
#endif

#if (defined __ios__) && defined QGC_SPEECH_ENABLED
extern void iOSSpeak(QString msg);
#endif

// Speech synthesis is only supported with MSVC compiler
#if defined _MSC_VER && defined QGC_SPEECH_ENABLED
// Documentation: http://msdn.microsoft.com/en-us/library/ee125082%28v=VS.85%29.aspx
#include <sapi.h>
#endif

#if defined Q_OS_LINUX && !defined __android__ && defined QGC_SPEECH_ENABLED
// Using eSpeak for speech synthesis: following https://github.com/mondhs/espeak-sample/blob/master/sampleSpeak.cpp
#include <espeak/speak_lib.h>
#endif

#define QGC_GAUDIOOUTPUT_KEY QString("QGC_AUDIOOUTPUT_")

QGCAudioWorker::QGCAudioWorker(QObject *parent) :
    QObject(parent),
    voiceIndex(0),
    #if defined _MSC_VER && defined QGC_SPEECH_ENABLED
    pVoice(NULL),
    #endif
    emergency(false),
    muted(false)
{
    // Load settings
    QSettings settings;
    muted = settings.value(QGC_GAUDIOOUTPUT_KEY + "muted", muted).toBool();
}

void QGCAudioWorker::init()
{
#if defined Q_OS_LINUX && !defined __android__ && defined QGC_SPEECH_ENABLED
    espeak_Initialize(AUDIO_OUTPUT_PLAYBACK, 500, NULL, 0); // initialize for playback with 500ms buffer and no options (see speak_lib.h)
    espeak_VOICE *espeak_voice = espeak_GetCurrentVoice();
    espeak_voice->languages = "en-uk"; // Default to British English
    espeak_voice->identifier = NULL; // no specific voice file specified
    espeak_voice->name = "klatt"; // espeak voice name
    espeak_voice->gender = 2; // Female
    espeak_voice->age = 0; // age not specified
    espeak_SetVoiceByProperties(espeak_voice);
#endif

#if defined _MSC_VER && defined QGC_SPEECH_ENABLED

    if (FAILED(::CoInitialize(NULL)))
    {
        qDebug() << "ERROR: Creating COM object for audio output failed!";
    }
    else
    {

        HRESULT hr = CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_ALL, IID_ISpVoice, (void **)&pVoice);

        if (FAILED(hr))
        {
            qDebug() << "ERROR: Initializing voice for audio output failed!";
        }
    }

#endif
}

QGCAudioWorker::~QGCAudioWorker()
{
#if defined _MSC_VER && defined QGC_SPEECH_ENABLED
    if (pVoice) {
        pVoice->Release();
        pVoice = NULL;
    }
    ::CoUninitialize();
#endif
}

void QGCAudioWorker::say(QString inText)
{
#ifdef __android__
    Q_UNUSED(inText);
#else
    static bool threadInit = false;
    if (!threadInit) {
        threadInit = true;
        init();
    }

    if (!muted)
    {
        QString text = fixTextMessageForAudio(inText);

#if defined _MSC_VER && defined QGC_SPEECH_ENABLED
        HRESULT hr = pVoice->Speak(text.toStdWString().c_str(), SPF_DEFAULT, NULL);
        if (FAILED(hr)) {
            qDebug() << "Speak failed, HR:" << QString("%1").arg(hr, 0, 16);
        }
#elif defined Q_OS_LINUX && defined QGC_SPEECH_ENABLED
        // Set size of string for espeak: +1 for the null-character
        unsigned int espeak_size = strlen(text.toStdString().c_str()) + 1;
        espeak_Synth(text.toStdString().c_str(), espeak_size, 0, POS_CHARACTER, 0, espeakCHARS_AUTO, NULL, NULL);
        espeak_Synchronize();
#elif (defined __macos__) && defined QGC_SPEECH_ENABLED
        macSpeak(text.toStdString().c_str());
#elif (defined __ios__) && defined QGC_SPEECH_ENABLED
        iOSSpeak(text);
#else
        // Make sure there isn't an unused variable warning when speech output is disabled
        Q_UNUSED(inText);
#endif
    }
#endif // __android__
}

void QGCAudioWorker::mute(bool mute)
{
    if (mute != muted)
    {
        this->muted = mute;
        QSettings settings;
        settings.setValue(QGC_GAUDIOOUTPUT_KEY + "muted", this->muted);
        //        emit mutedChanged(muted);
    }
}

bool QGCAudioWorker::isMuted()
{
    return this->muted;
}

bool QGCAudioWorker::_getMillisecondString(const QString& string, QString& match, int& number) {
    static QRegularExpression re("([0-9]+ms)");
    QRegularExpressionMatchIterator i = re.globalMatch(string);
    while (i.hasNext()) {
        QRegularExpressionMatch reMatch = i.next();
        if (reMatch.hasMatch()) {
            match = reMatch.captured(0);
            number = reMatch.captured(0).replace("ms", "").toInt();
            return true;
        }
    }
    return false;
}

QString QGCAudioWorker::fixTextMessageForAudio(const QString& string) {
    QString match;
    QString newNumber;
    QString result = string;

    //-- Look for codified terms
    if(result.contains(QStringLiteral("ERR "), Qt::CaseInsensitive)) {
        result.replace(QStringLiteral("ERR "), tr("error "), Qt::CaseInsensitive);
    }
    if(result.contains(QStringLiteral("ERR:"), Qt::CaseInsensitive)) {
        result.replace(QStringLiteral("ERR:"), tr("error."), Qt::CaseInsensitive);
    }
    if(result.contains(QStringLiteral("POSCTL"), Qt::CaseInsensitive)) {
        result.replace(QStringLiteral("POSCTL"), tr("Position Control"), Qt::CaseInsensitive);
    }
    if(result.contains(QStringLiteral("ALTCTL"), Qt::CaseInsensitive)) {
        result.replace(QStringLiteral("ALTCTL"), tr("Altitude Control"), Qt::CaseInsensitive);
    }
    if(result.contains(QStringLiteral("AUTO_RTL"), Qt::CaseInsensitive)) {
        result.replace(QStringLiteral("AUTO_RTL"), tr("auto Return To Launch"), Qt::CaseInsensitive);
    } else if(result.contains(QStringLiteral("RTL"), Qt::CaseInsensitive)) {
        result.replace(QStringLiteral("RTL"), tr("Return To Launch"), Qt::CaseInsensitive);
    }
    if(result.contains(QStringLiteral("ACCEL "), Qt::CaseInsensitive)) {
        result.replace(QStringLiteral("ACCEL "), tr("accelerometer "), Qt::CaseInsensitive);
    }
    if(result.contains(QStringLiteral("RC_MAP_MODE_SW"), Qt::CaseInsensitive)) {
        result.replace(QStringLiteral("RC_MAP_MODE_SW"), tr("RC mode switch"), Qt::CaseInsensitive);
    }
    if(result.contains(QStringLiteral("REJ."), Qt::CaseInsensitive)) {
        result.replace(QStringLiteral("REJ."), tr("Rejected"), Qt::CaseInsensitive);
    }
    if(result.contains(QStringLiteral("WP"), Qt::CaseInsensitive)) {
        result.replace(QStringLiteral("WP"), tr("way point"), Qt::CaseInsensitive);
    }
    if(result.contains(QStringLiteral("CMD"), Qt::CaseInsensitive)) {
        result.replace(QStringLiteral("CMD"), tr("command"), Qt::CaseInsensitive);
    }
    if(result.contains(QStringLiteral("COMPID"), Qt::CaseInsensitive)) {
        result.replace(QStringLiteral("COMPID"), tr("component eye dee"), Qt::CaseInsensitive);
    }
    if(result.contains(QStringLiteral(" params "), Qt::CaseInsensitive)) {
        result.replace(QStringLiteral(" params "), tr(" parameters "), Qt::CaseInsensitive);
    }
    if(result.contains(QStringLiteral(" id "), Qt::CaseInsensitive)) {
        result.replace(QStringLiteral(" id "), QStringLiteral(" eye dee "), Qt::CaseInsensitive);
    }
    if(result.contains(QStringLiteral(" ADSB "), Qt::CaseInsensitive)) {
        result.replace(QStringLiteral(" ADSB "), QStringLiteral(" Hey Dee Ess Bee "), Qt::CaseInsensitive);
    }

    // Convert negative numbers
    QRegularExpression re(QStringLiteral("(-)[0-9]*\\.?[0-9]"));
    QRegularExpressionMatch reMatch = re.match(result);
    while (reMatch.hasMatch()) {
        if (!reMatch.captured(1).isNull()) {
            // There is a negative prefix
            qDebug() << "negative" << reMatch.captured(1) << reMatch.capturedStart(1) << reMatch.capturedEnd(1);
            result.replace(reMatch.capturedStart(1), reMatch.capturedEnd(1) - reMatch.capturedStart(1), tr(" negative "));
            qDebug() << result;
        }
        reMatch = re.match(result);
    }

    // Convert meter postfix after real number
    re.setPattern(QStringLiteral("[0-9]*\\.?[0-9]\\s?(m)([^A-Za-z]|$)"));
    reMatch = re.match(result);
    while (reMatch.hasMatch()) {
        if (!reMatch.captured(1).isNull()) {
            // There is a meter postfix
            qDebug() << "meters" << reMatch.captured(1) << reMatch.capturedStart(1) << reMatch.capturedEnd(1);
            result.replace(reMatch.capturedStart(1), reMatch.capturedEnd(1) - reMatch.capturedStart(1), tr(" meters"));
            qDebug() << result;
        }
        reMatch = re.match(result);
    }

    int number;
    if(_getMillisecondString(string, match, number) && number > 1000) {
        if(number < 60000) {
            int seconds = number / 1000;
            newNumber = tr("%1 second%2").arg(seconds).arg(seconds > 1 ? "s" : "");
        } else {
            int minutes = number / 60000;
            int seconds = (number - (minutes * 60000)) / 1000;
            if (!seconds) {
                newNumber = tr("%1 minute%2").arg(minutes).arg(minutes > 1 ? "s" : "");
            } else {
                newNumber = tr("%1 minute%2 and %3 second%4").arg(minutes).arg(minutes > 1 ? "s" : "").arg(seconds).arg(seconds > 1 ? "s" : "");
            }
        }
        result.replace(match, newNumber);
    }
    // qDebug() << "Speech: " << result;
    return result;
}
