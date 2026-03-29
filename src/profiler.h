#pragma once
#include "config.h"
#include <string>
#include <vector>
#include <unordered_map>
#include "../dependencies/imgui/imgui.h"

struct ProfilerEntry
{
    std::string name;
    unsigned int queryID[2]; // start and end query
    float lastTimeMs = 0.0f;
};

class Profiler
{
public:
    void Begin(const std::string& name)
    {
	    if (entries.find(name) == entries.end())
	    {
		    ProfilerEntry entry;
		    entry.name = name;
		    glGenQueries(2, entry.queryID);
		    entries[name] = entry;
		    order.push_back(name);
	    }
	    glQueryCounter(entries[name].queryID[0], GL_TIMESTAMP);
    }

    void End(const std::string& name)
    {
	    glQueryCounter(entries[name].queryID[1], GL_TIMESTAMP);
    }

    // Call once per frame BEFORE Begin() calls to collect last frame's results
    void Collect()
    {
	    for (auto& [name, entry] : entries)
	    {
		    GLuint64 startTime, endTime;
		    glGetQueryObjectui64v(entry.queryID[0], GL_QUERY_RESULT, &startTime);
		    glGetQueryObjectui64v(entry.queryID[1], GL_QUERY_RESULT, &endTime);
		    entry.lastTimeMs = (endTime - startTime) / 1000000.0f;
	    }
    }


    void DrawGUI()
    {
	    // Draw in insertion order
	    for (const auto& name : order)
	    {
		    ImGui::Text("%-20s %.3f ms", name.c_str(), entries[name].lastTimeMs);
	    }
    }
private:
    std::unordered_map<std::string, ProfilerEntry> entries;
    std::vector<std::string> order;
};