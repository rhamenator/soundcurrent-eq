// SPDX-License-Identifier: GPL-3.0-only
// Development-only, deterministic admission of measured responses. Never accesses audio hardware.
#include "equipment_profiles.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <algorithm>
#include <stdexcept>
#include <cmath>
#include <cstdio>
using namespace soundcurrent;
using namespace soundcurrent::equipment;
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (argc != 3) { std::fprintf(stderr, "Usage: soundcurrent-profile-compiler INPUT.json OUTPUT.json\n"); return 2; }
    try {
        QFile input(argv[1]);
        if (!input.open(QIODevice::ReadOnly) || input.size() > 64*1024*1024) throw std::runtime_error("Cannot read bounded input");
        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson(input.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !doc.isArray() || doc.array().size()>4096) throw std::runtime_error("Expected bounded profile array");
        QJsonArray accepted, rejected;
        for (const auto &value : doc.array()) {
            try {
                auto candidate = value.toObject();
                // Input contains measured response only: never trust or reuse supplied filters.
                candidate["filters"] = QJsonArray{QJsonObject{{"type","PK"},{"frequency",1000},{"gain",0},{"q",1}}};
                auto p = parse(QJsonDocument(candidate).toJson());
                p.filters = fitResponse(p.response);
                // Individual-filter limits alone do not bound a stack of overlapping filters.
                // Conservatively scale the whole correction to at most +/-6 dB at any sampled point.
                for (int attempt=0; attempt<6; ++attempt) {
                    double maximum=0;
                    for(int i=0;i<=2048;++i) {
                        const double hz=20*std::pow(1000.,i/2048.); double db=0;
                        for(const auto &b:p.filters) db+=filterResponseDb(b,48000,hz);
                        maximum=std::max(maximum,std::abs(db));
                    }
                    if(maximum<=6.00001)break;
                    for(auto &b:p.filters)b.gainDb*=5.95/maximum;
                }
                double before=0,after=0,maximum=0;
                for(const auto &point:p.response) {
                    double correction=0;
                    for(const auto &b:p.filters)correction+=filterResponseDb(b,48000,point.frequency);
                    before+=point.db*point.db;after+=std::pow(point.db+correction,2);
                }
                for(int i=0;i<=2048;++i) {
                    double db=0;for(const auto &b:p.filters)db+=filterResponseDb(b,48000,20*std::pow(1000.,i/2048.));
                    maximum=std::max(maximum,std::abs(db));
                }
                if(!std::isfinite(before+after+maximum) || maximum>6.001 || !(after<before*.85))
                    throw std::runtime_error("Correction fails residual improvement / composite gain admission");
                auto o=serialize(p);
                o["fitMetrics"]=QJsonObject{{"beforeRmsDb",std::sqrt(before/p.response.size())},{"afterRmsDb",std::sqrt(after/p.response.size())},{"maximumCorrectionDb",maximum},{"sampleRate",48000}};
                accepted.append(o);
            } catch(const std::exception &e) { rejected.append(QJsonObject{{"id",value.toObject().value("id")},{"reason",e.what()}}); }
        }
        QSaveFile output(argv[2]);
        const auto bytes=QJsonDocument(QJsonObject{{"accepted",accepted},{"rejected",rejected}}).toJson();
        if(!output.open(QIODevice::WriteOnly) || output.write(bytes)!=bytes.size() || !output.commit())throw std::runtime_error("Cannot atomically write output");
        std::printf("Admitted %lld / %lld measured corrections\n",static_cast<long long>(accepted.size()),static_cast<long long>(doc.array().size()));
    } catch(const std::exception &e) { std::fprintf(stderr,"%s\n",e.what());return 1; }
}
