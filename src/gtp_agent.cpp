#include "gtp_agent.h"
#include <iostream>
#include <algorithm>
#include <functional>
#include <cctype>
#include <sstream>
#include <cassert>

inline void trim(std::string &ss)   
{   
	auto p=std::find_if(ss.rbegin(),ss.rend(), std::not1(std::ptr_fun(::isspace)));   
    ss.erase(p.base(),ss.end());  
	auto p2 = std::find_if(ss.begin(),ss.end(), std::not1(std::ptr_fun(::isspace)));   
	ss.erase(ss.begin(), p2);
} 

inline vector<string> split_str(const string& name, char delim, bool strip=true) {

    std::stringstream ss(name);
    std::string item;
    std::vector<std::string> names;
    while (std::getline(ss, item, delim)) {
        names.push_back(item);
    }

    if (strip) {
        if (names.size() && names.front().empty())
            names.erase(names.begin());

        if (names.size() && names.back().empty())
            names.erase(names.end()-1); 
    }

    return names;
}


GtpAgent::GtpAgent(const string& cmdline, const string& path)
{
    if (!cmdline.empty())
        execute(cmdline, path);
}

void GtpAgent::clear_vars() {

    clean_command_queue();
    support_commands_.clear();
    ready_ = false;
    ready_query_made_ = false; 
    board_size_ = 19;
    recvbuffer_.clear(); 

    handicaps_.clear();
    history_moves_.clear();
}

void GtpAgent::execute(const string& cmdline, const string& path) {

    command_line_ = cmdline;
    path_ = path;

    clear_vars();

    process_ = make_shared<Process>(command_line_, path_, [this](const char *bytes, size_t n) {
        
        if (onOutput) {
            // skip ready query commamd response
            if (ready_query_made_)
                onOutput(string(bytes, n));
        }

        if (recvbuffer_.empty() && bytes[0] != '=' && bytes[0] != '?') {
            if (onUnexpectOutput)
                onUnexpectOutput(string(bytes, n));

            return;
        }

        // there may be multiple response at same
        int cr = 0;
        if (recvbuffer_.size() && recvbuffer_.back() == '\n')
            cr++;

        for (size_t i=0; i<n; i++) {
            if (bytes[i] == '\n') cr++;
            else cr = 0;

            
            if (recvbuffer_.empty() && bytes[i] == '\n') {
                // skip multiple space 
            }
            else
                recvbuffer_ += bytes[i];

            if (cr == 2) {
                cr = 0;
                auto line = recvbuffer_;
                recvbuffer_ = "";

                // new response
                int id = -1;
                auto ws_pos = line.find(' ');
                if (std::isdigit(line[1]))
                    id = stoi(line.substr(1, ws_pos));

                auto rsp = line.substr(ws_pos+1);
                trim(rsp);

                command_t cmd;
                command_queue_.try_pop(cmd);
                bool success = line[0] == '=';
                onGtpResult(id, success, cmd.cmd, rsp);

                if (cmd.handler)
                    cmd.handler(success, rsp);
            }
        }

    }, [this](const char *bytes, size_t n) {
        if (onStderr)
            onStderr(string(bytes, n));
    }, true);


    send_command("protocol_version");
    send_command("name");
    send_command("version");
    send_command("list_commands");
}

bool GtpAgent::isReady() const {
    return ready_;
}


string GtpAgent::pending_command() {
    command_t cmd;
    command_queue_.try_peek(cmd);
    return cmd.cmd;
}

void GtpAgent::onGtpResult(int, bool success, const string& cmd, const string& rsp) {

    if (!ready_query_made_) {

        if (!success) {
            kill();
            clean_command_queue();
            return;
        }

        if (cmd == "list_commands") {

            // update only at first hit
            ready_query_made_ = true;
            std::lock_guard<std::mutex> lk(mtx_); 
            support_commands_ = split_str(rsp, '\n'); 
            ready_ = true;
            onReset();
        }
        else if (cmd == "protocol_version")
            protocol_version_ = rsp;
        else if (cmd == "version")
            version_ = rsp;
        else if (cmd == "name")
            name_ = rsp;
    
        return;
    }

    if (success) {
        if (cmd.find("boardsize") == 0) {

            std::istringstream cmdstream(cmd);
            std::string stmp;
            int bdsize;

            cmdstream >> stmp;  // eat boardsize
            cmdstream >> bdsize;
            board_size_ = bdsize;
        }
        else if (cmd.find("clear_board") == 0) {
            handicaps_.clear();
            history_moves_.clear();
            onReset();
        }
        else if (cmd.find("play") == 0) {
            std::istringstream cmdstream(cmd);
            std::string tmp;
            std::string color, vertex;

            cmdstream >> tmp;   //eat play
            cmdstream >> color;
            cmdstream >> vertex;

            bool is_black = color[0] == 'b';
            auto pos = text_to_move(vertex);
            history_moves_.push_back({is_black, pos});
        }
        else if (cmd.find("genmove") == 0 || cmd.find("kgs-genmove_cleanup") == 0) {
            std::istringstream cmdstream(cmd);
            std::string tmp;
            std::string color;

            cmdstream >> tmp;   //eat genmove
            cmdstream >> color;

            bool is_black = color[0] == 'b';
            auto pos = text_to_move(rsp);
            history_moves_.push_back({is_black, pos});
        }
        else if (cmd.find("undo") == 0) {
            if (history_moves_.size())
                history_moves_.erase(history_moves_.end()-1);
        }
        else if (cmd.find("handicap") != string::npos) {

            // D3 D4 ...
            std::istringstream cmdstream(rsp);
            do {
                std::string vertex;

                cmdstream >> vertex;

                if (!cmdstream.fail()) {
                    auto pos = text_to_move(vertex);
                    if (pos == invalid_move)
                        break;
                    handicaps_.push_back(pos);
                }
            } while (!cmdstream.fail());
        }
    }
}

bool GtpAgent::alive() {
    int exit_status;
    return process_ && !process_->try_get_exit_status(exit_status);
}

bool GtpAgent::support(const string& cmd) {
    std::lock_guard<std::mutex> lk(mtx_);  
    return find(support_commands_.begin(), support_commands_.end(), cmd) != support_commands_.end();   
}


void GtpAgent::clean_command_queue() {

    command_t cmd;
    while (command_queue_.try_pop(cmd)) {
        if (cmd.handler)
            cmd.handler(false, "not active");
    }
}

void GtpAgent::send_command(const string& cmd, function<void(bool, const string&)> handler) {

    if (!alive()) {
        if (handler) {
            handler(false, "not active");
        }
        return;
    }

    {
        std::lock_guard<std::mutex> lk(mtx_);  
        command_queue_.push({cmd, handler});
        process_->write(cmd+"\n");
    }

    if (onInput)
        onInput(cmd);
}

string GtpAgent::send_command_sync(const string& cmd, bool& success, int timeout_secs) {

    string ret;
    std::atomic<bool> returned{false};

    send_command(cmd, [&success, &ret, &returned](bool ok, const string& out) {

        success = ok;
        ret = out;
        returned = true;
    });

    if (!returned)
        this_thread::sleep_for(chrono::microseconds(10)); 

    if (!returned)
        this_thread::sleep_for(chrono::microseconds(100)); 

    int ecplised = 0;
    while (!returned) {
        
        if (!alive()) {
            success = false;
            return "? not active";
        }
        this_thread::sleep_for(chrono::microseconds(1)); 

        if (!returned && timeout_secs > 0 && ecplised++ >= timeout_secs) {
            success = false;
            return "? timeout";
        }
    }

    return success ? ret : ("? ") + ret;
}

string GtpAgent::send_command_sync(const string& cmd, int timeout_secs) {
    bool success;
    return send_command_sync(cmd, success, timeout_secs);
}

void GtpAgent::kill() {
    if (alive()) 
        process_->kill();
}


int GtpAgent::text_to_move(const string& vertex) const {

    if (vertex == "pass" || vertex == "PASS") return GtpAgent::pass_move;
    else if (vertex == "resign" || vertex == "RESIGN") return GtpAgent::resign_move;

    if (vertex.size() < 2) return GtpAgent::invalid_move;
    if (!std::isalpha(vertex[0])) return GtpAgent::invalid_move;
    if (!std::isdigit(vertex[1])) return GtpAgent::invalid_move;
    if (vertex[0] == 'i') return GtpAgent::invalid_move;

    int column, row;
    if (vertex[0] >= 'A' && vertex[0] <= 'Z') {
        if (vertex[0] < 'I') {
            column = vertex[0] - 'A';
        } else {
            column = (vertex[0] - 'A')-1;
        }
    } else {
        if (vertex[0] < 'i') {
            column = vertex[0] - 'a';
        } else {
            column = (vertex[0] - 'a')-1;
        }
    }

    std::string rowstring(vertex);
    rowstring.erase(0, 1);
    std::istringstream parsestream(rowstring);

    parsestream >> row;
    row--;

    if (row >= board_size_ || column >= board_size_) {
        return GtpAgent::invalid_move;
    }

    auto move = row * board_size_ + column;
    return move;
}

static string move_to_text(int move, const int board_size) {

    std::ostringstream result;

    int column = move % board_size;
    int row = move / board_size;

    assert(move == GtpAgent::pass_move || move == GtpAgent::resign_move || (row >= 0 && row < board_size));
    assert(move == GtpAgent::pass_move || move == GtpAgent::resign_move || (column >= 0 && column < board_size));

    if (move >= 0) {
        result << static_cast<char>(column < 8 ? 'A' + column : 'A' + column + 1);
        result << (row + 1);
    } else if (move == GtpAgent::pass_move) {
        result << "pass";
    } else if (move == GtpAgent::resign_move) {
        result << "resign";
    } else {
        result << "error";
    }

    return result.str();
}



bool GtpAgent::wait_till_ready(int secs) {

    int passed = 0;
    while (!isReady()) {
        this_thread::sleep_for(chrono::seconds(1));
        if (passed++ >= secs)
            break;
    }
    return isReady();
}

bool GtpAgent::restore(int secs) {

    if (!alive()) {

        auto handicaps = handicaps_;
        auto history_moves = history_moves_;
        auto bdsize = board_size_;

        execute(command_line_, path_);
        onReset();

        if (!wait_till_ready(secs))
            return false;

        send_command("boardsize " + to_string(bdsize));

        for (auto pos : handicaps) {
            send_command("set_free_handicap " + move_to_text(pos, bdsize));
        }

        for (auto& m : history_moves)
            send_command("play " + string(m.is_black? "b " :"w ") + move_to_text(m.pos, bdsize));
    }

    return true;
}

void GtpAgent::quit() {
    send_command("quit");
}   

bool GtpAgent::next_move_is_black() const {

    return (handicaps_.empty() && history_moves_.empty()) ||
            (history_moves_.size() && !history_moves_.back().is_black);
}



GameAdvisor::GameAdvisor(const string& cmdline, const string& path)
: GtpAgent(cmdline, path)
{
    onInput = [this](const string& line) {
        events_.push("input;" + line);
    };
    onOutput = [this](const string& line) {
        events_.push("output;" + line);
    };
}

void GameAdvisor::onReset() {

    reset_vars();

    events_.push("reset;");
}

void GameAdvisor::think(bool black_move) {

    events_.push("think;");

    send_command(black_move ? "genmove b" : "genmove w", [black_move, this](bool success, const string& rsp) {

        if (!success)
            return;

        int move = text_to_move(rsp);
        if (move == invalid_move)
            throw std::runtime_error("unexpected mvoe " + to_string(move));

        if (!pending_reset_) {

            string player = black_move ? "b;" : "w;";
            events_.push("think_end;");
            if (move == pass_move)
                events_.push("pass;" + player);
            else if (move == resign_move)
                events_.push("resign;" + player);
            else {
                std::stringstream ss;
                ss << "move;";
                ss << player;
                ss << move << ";";
                events_.push(ss.str());
            }

            commit_pending_ = true;
            commit_player_ = black_move;
            commit_pos_ = move;
        }
    });
}

void GameAdvisor::place(bool black_move, int pos) {

    events_.push("update_board;");

    if (commit_pending_) {

        commit_pending_ = false;

        if (black_move == commit_player_ &&
            pos == commit_pos_) {
            return;
        }

        send_command("undo"); // not my turn or not play as sugessed, undo genmove
    }

    put_stone(black_move, pos);
}

void GameAdvisor::put_stone(bool black_move, int pos) {

    auto mtext = move_to_text(pos, board_size_);
    if (mtext.empty()) {
        // if (handler) handler(GtpAgent::invalid_move);
        return;
    }

    string cmd = black_move ? "play b " : "play w ";
    cmd += mtext;

    send_command(cmd, [black_move, pos, this](bool success, const string&) {

        if (!success) return; // throw std::runtime_error("unexpected play result " + to_string(ret));

        if (!pending_reset_) {

            if (commit_pending_) {
                commit_pending_ = false;

                // duplicated !!!
                // someone play the stone before think return 
                send_command("undo");  // undo myself
                send_command("undo");  // undo genmove
                // replay myself
                put_stone(black_move, pos);
                return;
            }

            if (my_side_is_black_ == !black_move)
                think(my_side_is_black_);
        }
    });
}



void GameAdvisor::pop_events() {

    string ev;
	while (events_.try_pop(ev)) {
        auto params = split_str(ev, ';');
        if (params[0] == "move") {
            bool black_move = params[1] == "b";
            int move = stoi(params[2]);
            if (onThinkMove)
				onThinkMove(black_move, move);
        }
        else if (params[0] == "pass") {
            if (onThinkPass)
                onThinkPass();
        }
        else if (params[0] == "resign") {
            if (onThinkResign)
                onThinkResign();
        }
        else if (params[0] == "think") {
            if (onThinkBegin)
                onThinkBegin();
        }
        else if (params[0] == "think_end") {
            if (onThinkEnd)
                onThinkEnd();
        }
        else if (params[0] == "input") {
            if (onGtpIn)
                onGtpIn(ev.substr(6));
        }
        else if (params[0] == "output") {
            if (onGtpOut)
                onGtpOut(ev.substr(7));
        }
        else if (params[0] == "update_board") {
            if (onMoveChange)
                onMoveChange();
        }
    }
}

void GameAdvisor::reset(bool my_side) {

    send_command("clear_board");
    pending_reset_ = true;

    if (my_side)
        hint();
}

void GameAdvisor::reset_vars() {
    while (!events_.empty()) events_.try_pop();
    pending_reset_ = false;
    commit_pending_ = false;
    my_side_is_black_ = true;
}


void GameAdvisor::hint() {

    auto pending = pending_command();
    if (commit_pending_ || pending.find("genmove ") == 0 || pending.find("play ") == 0)
        return;

    my_side_is_black_ = next_move_is_black();
    think(my_side_is_black_);
}