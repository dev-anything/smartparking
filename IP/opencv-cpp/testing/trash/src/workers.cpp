#include "workers.h"
#include "constants.h"
#include "preprocess.h"
#include "detector.h"
#include "rectify.h"
#include "recognizer.h"

#include <ctime>
#include <fstream>
#include <iostream>
#include <map>

namespace
{
    std::string format_time(const TimePoint& t, const char* fmt)
    {
        std::time_t tt = std::chrono::system_clock::to_time_t(t);
        std::tm tm;
        localtime_r(&tt, &tm);
        char buf[64];
        std::strftime(buf, sizeof(buf), fmt, &tm);
        return buf;
    }

    // 캡처본 1장에서 번호판 후보를 모두 찾아 인식 → 통과한 결과만 반환
    std::vector<PlateResult> recognize_frame(const cv::Mat& frame)
    {
        cv::Mat pre = preprocess(frame);

        std::vector<PlateCandidate> cands = detect_by_rect(pre);
        std::vector<PlateCandidate> color = detect_by_color(frame);
        cands.insert(cands.end(), color.begin(), color.end());
        //cands = remove_duplicates(cands);

        std::vector<PlateResult> results;
        for (const auto& c : cands)
        {
            cv::Mat plate = rectify_plate(frame, c.corners);
            if (plate.empty()) continue;
            PlateResult r = recognizer(plate);
            if (r.valid && r.confidence >= OCR_MIN_CONF) results.push_back(r);
        }
        return results;
    }
}

// 차 1대분 캡처본 묶음 → 번호판 1개로 확정
// 여러 장의 결과를 투표: 가장 많이 나온 번호, 같으면 신뢰도 높은 쪽
void recognition_loop(CaptureQueue& in, RecordQueue& out)
{
    CaptureEvent ev;
    while (in.pop(ev))
    {
        struct Vote { int count; float best_conf; std::size_t frame_idx; };
        std::map<std::string, Vote> votes;

        for (std::size_t i = 0; i < ev.frames.size(); i++)
        {
            for (const auto& r : recognize_frame(ev.frames[i]))
            {
                auto it = votes.find(r.text);
                if (it == votes.end())
                    votes[r.text] = Vote{ 1, r.confidence, i };
                else
                {
                    it->second.count++;
                    if (r.confidence > it->second.best_conf)
                    {
                        it->second.best_conf = r.confidence;
                        it->second.frame_idx = i;
                    }
                }
            }
        }

        PlateRecord rec;
        rec.camera     = ev.camera;
        rec.id         = ev.id;
        rec.time       = ev.time;                            // 통과 시작 시각
        rec.plate      = "";
        rec.confidence = 0.0f;
        rec.valid      = false;
        rec.evidence   = ev.frames[ev.frames.size() / 2];    // 실패 시: 가운데 장면

        const Vote* best = nullptr;
        for (const auto& kv : votes)
        {
            const Vote& v = kv.second;
            if (!best || v.count > best->count ||
                (v.count == best->count && v.best_conf > best->best_conf))
            {
                best           = &v;
                rec.plate      = kv.first;
                rec.confidence = v.best_conf;
                rec.valid      = true;
                rec.evidence   = ev.frames[v.frame_idx];     // 가장 잘 읽힌 장면
            }
        }

        out.push(std::move(rec));
    }
}

void sender_loop(RecordQueue& in)
{
    std::ofstream log("records/plates.txt", std::ios::app);
    if (!log) std::cerr << "[전송] 기록 파일 열기 실패: records/plates.txt" << std::endl;

    PlateRecord rec;
    while (in.pop(rec))
    {
        std::string when  = format_time(rec.time, "%Y-%m-%d %H:%M:%S");
        std::string stamp = format_time(rec.time, "%Y%m%d_%H%M%S");
        std::string plate = rec.valid ? rec.plate : "UNKNOWN";

        // 1. 증거 사진
        std::string img = "records/" + stamp + "_" + rec.camera + "_" + std::to_string(rec.id) + ".jpg";
        cv::imwrite(img, rec.evidence);

        // 2. 기록 파일 (한 줄씩, 바로 디스크에)
        if (log)
            log << when << '\t' << rec.camera << '\t' << plate << '\t'
                << rec.confidence << '\t' << img << std::endl;

        // 3. 서버 전송 (TODO: connect_server / send)
        std::cout << "[SEND] " << rec.camera << " " << plate << " " << when << std::endl;
    }
}