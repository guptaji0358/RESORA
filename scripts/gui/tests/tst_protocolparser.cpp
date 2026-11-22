#include <QJsonObject>
#include <QSignalSpy>
#include <QtTest>

#include "backend/protocolparser.h"

class TstProtocolParser : public QObject
{
    Q_OBJECT
private slots:
    void progressWithTotal()
    {
        ProtocolParser p;
        QSignalSpy spy(&p, &ProtocolParser::progress);
        p.feed("{\"v\":1,\"type\":\"progress\",\"done\":3,\"total\":10,\"message\":\"a.png\"}\n");
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][0].toLongLong(), 3);
        QCOMPARE(spy[0][1].toLongLong(), 10);
        QCOMPARE(spy[0][2].toString(), QString("a.png"));
    }
    void progressUnknownTotalAndNullMessage()
    {
        ProtocolParser p;
        QSignalSpy spy(&p, &ProtocolParser::progress);
        p.feed("{\"v\":1,\"type\":\"progress\",\"done\":0,\"total\":null,\"message\":null}\n");
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][1].toLongLong(), -1);
        QVERIFY(spy[0][2].toString().isEmpty());
    }
    void partialChunks()
    {
        ProtocolParser p;
        QSignalSpy spy(&p, &ProtocolParser::progress);
        p.feed("{\"v\":1,\"type\":\"prog");
        p.feed("ress\",\"done\":1,\"total\":2,\"mess");
        QCOMPARE(spy.count(), 0);
        p.feed("age\":null}\n");
        QCOMPARE(spy.count(), 1);
    }
    void multipleMessagesInOneChunk()
    {
        ProtocolParser p;
        QSignalSpy prog(&p, &ProtocolParser::progress);
        QSignalSpy res(&p, &ProtocolParser::result);
        p.feed("{\"v\":1,\"type\":\"progress\",\"done\":1,\"total\":2,\"message\":null}\r\n"
               "{\"v\":1,\"type\":\"progress\",\"done\":2,\"total\":2,\"message\":null}\n"
               "{\"v\":1,\"type\":\"result\",\"data\":{\"k\":1}}\n");
        QCOMPARE(prog.count(), 2);
        QCOMPARE(res.count(), 1);
        QCOMPARE(res[0][0].toJsonValue().toObject().value("k").toInt(), 1);
        QVERIFY(p.hasFinal());
    }
    void errorEnvelope()
    {
        ProtocolParser p;
        QSignalSpy spy(&p, &ProtocolParser::errorEnvelope);
        p.feed("{\"v\":1,\"type\":\"error\",\"code\":\"invalid_input\",\"message\":\"nope\"}\n");
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][0].toString(), QString("invalid_input"));
        QCOMPARE(spy[0][1].toString(), QString("nope"));
    }
    void malformedJsonIsViolationNotCrash()
    {
        ProtocolParser p;
        QSignalSpy spy(&p, &ProtocolParser::violation);
        p.feed("this is not json\n");
        p.feed("{\"v\":1,\"type\":\"result\",\"data\":null}\n");  // ignored after violation
        QCOMPARE(spy.count(), 1);
        QVERIFY(p.hasViolation());
    }
    void wrongVersionIsViolation()
    {
        ProtocolParser p;
        QSignalSpy spy(&p, &ProtocolParser::violation);
        p.feed("{\"v\":2,\"type\":\"result\",\"data\":null}\n");
        QCOMPARE(spy.count(), 1);
    }
    void envelopeAfterFinalIsViolation()
    {
        ProtocolParser p;
        QSignalSpy spy(&p, &ProtocolParser::violation);
        p.feed("{\"v\":1,\"type\":\"result\",\"data\":null}\n"
               "{\"v\":1,\"type\":\"progress\",\"done\":1,\"total\":1,\"message\":null}\n");
        QCOMPARE(spy.count(), 1);
    }
    void unknownTypeIgnored()
    {
        ProtocolParser p;
        QSignalSpy unk(&p, &ProtocolParser::unknownType);
        QSignalSpy vio(&p, &ProtocolParser::violation);
        p.feed("{\"v\":1,\"type\":\"telemetry\"}\n");
        QCOMPARE(unk.count(), 1);
        QCOMPARE(vio.count(), 0);
        QVERIFY(!p.hasFinal());
    }
    void unterminatedValidFinalLineAccepted()
    {
        ProtocolParser p;
        QSignalSpy res(&p, &ProtocolParser::result);
        p.feed("{\"v\":1,\"type\":\"result\",\"data\":{}}");
        QCOMPARE(res.count(), 0);
        p.finish();
        QCOMPARE(res.count(), 1);
    }
    void truncatedFinalLineIsViolation()
    {
        ProtocolParser p;
        QSignalSpy vio(&p, &ProtocolParser::violation);
        p.feed("{\"v\":1,\"type\":\"progr");
        p.finish();
        QCOMPARE(vio.count(), 1);
        QVERIFY(vio[0][0].toString().contains("incomplete"));
    }
    void oversizedLineIsViolation()
    {
        ProtocolParser p;
        QSignalSpy vio(&p, &ProtocolParser::violation);
        p.feed(QByteArray(ProtocolParser::kMaxLineBytes + 1, 'x'));
        QCOMPARE(vio.count(), 1);
    }
};

QTEST_APPLESS_MAIN(TstProtocolParser)
#include "tst_protocolparser.moc"
