/*
 * ================================================================
 *  NeuroFit AI — C++ Backend Server
 *  Architecture: Full Object-Oriented Design (OOPS)
 * ================================================================
 *  OOPS Concepts Demonstrated:
 *  1.  Abstraction        — Abstract base classes with pure virtual methods
 *  2.  Encapsulation      — Private data, public interfaces, validation
 *  3.  Inheritance        — Multi-level class hierarchies
 *  4.  Polymorphism       — Virtual dispatch, runtime behavior
 *  5.  Templates          — Generic programming with type safety
 *  6.  Singleton Pattern  — DatabaseManager single instance
 *  7.  Factory Pattern    — PlanFactory object creation
 *  8.  Observer Pattern   — FitnessEvent notification system
 *  9.  Strategy Pattern   — Pluggable AI providers
 * 10.  Builder Pattern    — ProgressLog fluent construction
 * 11.  Command Pattern    — API request routing
 * ================================================================
 *  Dependencies (install via vcpkg):
 *    vcpkg install libpqxx nlohmann-json cpp-httplib
 * ================================================================
 */

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <stdexcept>
#include <algorithm>
#include <numeric>
#include <ctime>
#include <cmath>
#include <sstream>
#include <mutex>
#include <optional>
#include <variant>

// Third-party (uncomment when dependencies are installed)
// #include <pqxx/pqxx>
// #include <nlohmann/json.hpp>
// #include <httplib.h>

namespace NeuroFit {

// ================================================================
//  SECTION 1: ENUMS & VALUE TYPES
// ================================================================

enum class FitnessGoal {
    WEIGHT_LOSS, MUSCLE_GAIN, ENDURANCE,
    FLEXIBILITY, GENERAL_FITNESS, ATHLETIC_PERFORMANCE
};

enum class ActivityLevel {
    SEDENTARY, LIGHT, MODERATE, ACTIVE, VERY_ACTIVE
};

enum class Gender { MALE, FEMALE, OTHER };

enum class PlanType { WORKOUT, DIET };

enum class WorkoutType {
    FULL_BODY, UPPER_BODY, LOWER_BODY,
    PUSH, PULL, HIIT, YOGA, CORE, ATHLETIC
};

enum class DifficultyLevel { BEGINNER, INTERMEDIATE, ADVANCED, ELITE };

// ----------------------------------------------------------------
//  Macros Value Object — Immutable nutrition data container
// ----------------------------------------------------------------
class Macros {
private:
    double protein_g_;
    double carbs_g_;
    double fat_g_;

public:
    Macros(double p, double c, double f)
        : protein_g_(p), carbs_g_(c), fat_g_(f) {
        if (p < 0 || c < 0 || f < 0)
            throw std::invalid_argument("Macros cannot be negative");
    }

    // Getters (read-only access — encapsulation)
    double protein()   const { return protein_g_; }
    double carbs()     const { return carbs_g_; }
    double fat()       const { return fat_g_; }
    double totalCalories() const {
        return protein_g_ * 4.0 + carbs_g_ * 4.0 + fat_g_ * 9.0;
    }

    // Operator overloading
    Macros operator+(const Macros& other) const {
        return Macros(protein_g_ + other.protein_g_,
                      carbs_g_   + other.carbs_g_,
                      fat_g_     + other.fat_g_);
    }

    std::string toString() const {
        return "Protein: " + std::to_string((int)protein_g_) + "g | "
             + "Carbs: "   + std::to_string((int)carbs_g_)   + "g | "
             + "Fat: "     + std::to_string((int)fat_g_)     + "g";
    }
};

// ================================================================
//  SECTION 2: USER CLASS — Core domain entity with encapsulation
// ================================================================

class User {
private:
    // Private members — strict encapsulation
    std::string id_;
    std::string name_;
    int         age_;
    double      weight_kg_;
    double      height_cm_;
    FitnessGoal goal_;
    ActivityLevel activity_;
    Gender      gender_;
    std::string created_at_;

    // Private validation helpers
    void validateWeight(double w) {
        if (w < 10.0 || w > 600.0)
            throw std::invalid_argument("Weight must be between 10–600 kg");
    }
    void validateHeight(double h) {
        if (h < 50.0 || h > 280.0)
            throw std::invalid_argument("Height must be between 50–280 cm");
    }
    void validateAge(int a) {
        if (a < 10 || a > 120)
            throw std::invalid_argument("Age must be between 10–120 years");
    }

public:
    // Constructor with validation
    User(const std::string& id, const std::string& name,
         int age, double weight, double height,
         FitnessGoal goal, ActivityLevel activity,
         Gender gender = Gender::MALE)
        : id_(id), name_(name), age_(age), weight_kg_(weight),
          height_cm_(height), goal_(goal), activity_(activity), gender_(gender)
    {
        validateAge(age);
        validateWeight(weight);
        validateHeight(height);
    }

    // Public getters
    const std::string& getId()        const { return id_; }
    const std::string& getName()      const { return name_; }
    int         getAge()              const { return age_; }
    double      getWeight()           const { return weight_kg_; }
    double      getHeight()           const { return height_cm_; }
    FitnessGoal getGoal()             const { return goal_; }
    ActivityLevel getActivity()       const { return activity_; }
    Gender      getGender()           const { return gender_; }

    // Controlled setters with validation
    void setWeight(double w) { validateWeight(w); weight_kg_ = w; }
    void setGoal(FitnessGoal g) { goal_ = g; }
    void setActivity(ActivityLevel a) { activity_ = a; }

    // ── Domain Logic Methods ──

    // BMI Calculation
    double calcBMI() const {
        double h_m = height_cm_ / 100.0;
        return weight_kg_ / (h_m * h_m);
    }

    std::string getBMICategory() const {
        double b = calcBMI();
        if (b < 18.5) return "Underweight";
        if (b < 25.0) return "Normal";
        if (b < 30.0) return "Overweight";
        return "Obese";
    }

    // Mifflin-St Jeor Basal Metabolic Rate
    double calcBMR() const {
        double sex = (gender_ == Gender::FEMALE) ? -161.0 : 5.0;
        return 10.0 * weight_kg_ + 6.25 * height_cm_ - 5.0 * age_ + sex;
    }

    // Total Daily Energy Expenditure
    double calcTDEE() const {
        static const std::unordered_map<ActivityLevel, double> mult {
            {ActivityLevel::SEDENTARY,   1.20},
            {ActivityLevel::LIGHT,       1.375},
            {ActivityLevel::MODERATE,    1.55},
            {ActivityLevel::ACTIVE,      1.725},
            {ActivityLevel::VERY_ACTIVE, 1.90}
        };
        return calcBMR() * mult.at(activity_);
    }

    // Goal-adjusted calorie target
    double calcAdjustedCalories() const {
        static const std::unordered_map<FitnessGoal, double> adj {
            {FitnessGoal::WEIGHT_LOSS,         -500.0},
            {FitnessGoal::MUSCLE_GAIN,         +300.0},
            {FitnessGoal::ENDURANCE,           +200.0},
            {FitnessGoal::FLEXIBILITY,           0.0},
            {FitnessGoal::GENERAL_FITNESS,       0.0},
            {FitnessGoal::ATHLETIC_PERFORMANCE,+400.0}
        };
        return calcTDEE() + adj.at(goal_);
    }

    // Macro calculation based on goal
    Macros calcMacros() const {
        double kcal = calcAdjustedCalories();
        struct Ratio { double p, c, f; };
        static const std::unordered_map<FitnessGoal, Ratio> ratios {
            {FitnessGoal::WEIGHT_LOSS,         {0.35, 0.40, 0.25}},
            {FitnessGoal::MUSCLE_GAIN,         {0.30, 0.50, 0.20}},
            {FitnessGoal::ENDURANCE,           {0.20, 0.60, 0.20}},
            {FitnessGoal::FLEXIBILITY,         {0.25, 0.50, 0.25}},
            {FitnessGoal::GENERAL_FITNESS,     {0.25, 0.50, 0.25}},
            {FitnessGoal::ATHLETIC_PERFORMANCE,{0.30, 0.50, 0.20}}
        };
        const auto& r = ratios.at(goal_);
        return Macros(kcal * r.p / 4.0, kcal * r.c / 4.0, kcal * r.f / 9.0);
    }

    // Compute neural fitness score (0–100)
    int calcNeuralScore(int workoutCount, double avgSleep) const {
        int score = 50;
        double b = calcBMI();
        if (b >= 18.5 && b < 25.0) score += 20;
        else if (b >= 25.0 && b < 30.0) score += 5;
        score += std::min(20, workoutCount * 2);
        if (avgSleep >= 7.0 && avgSleep <= 9.0) score += 10;
        return std::min(100, score);
    }

    void print() const {
        std::cout << "──────────────────────────────────────\n"
                  << "  User: " << name_ << " (ID: " << id_ << ")\n"
                  << "  Age: " << age_ << "  |  Weight: " << weight_kg_ << " kg\n"
                  << "  Height: " << height_cm_ << " cm  |  BMI: " << calcBMI() << " (" << getBMICategory() << ")\n"
                  << "  BMR: " << (int)calcBMR() << " kcal  |  TDEE: " << (int)calcTDEE() << " kcal\n"
                  << "  Goal Calories: " << (int)calcAdjustedCalories() << " kcal\n"
                  << "  Macros: " << calcMacros().toString() << "\n"
                  << "──────────────────────────────────────\n";
    }
};

// ================================================================
//  SECTION 3: ABSTRACT BASE CLASS — FitnessEntity
//  Demonstrates: Abstraction + Template Method Pattern
// ================================================================

class FitnessEntity {
protected:
    std::string entity_id_;
    std::string user_id_;
    std::string title_;
    std::string content_;
    bool        is_active_;
    std::time_t created_at_;

public:
    FitnessEntity(const std::string& eid, const std::string& uid,
                  const std::string& title)
        : entity_id_(eid), user_id_(uid), title_(title),
          is_active_(true), created_at_(std::time(nullptr)) {}

    virtual ~FitnessEntity() = default;

    // Pure virtual methods — subclasses MUST implement (Abstraction)
    virtual PlanType    getType()    const = 0;
    virtual std::string getSummary() const = 0;
    virtual void        validate()   const = 0;
    virtual double      getScore()   const = 0;

    // Template Method — fixed algorithm, customizable hooks
    void activate() {
        is_active_ = true;
        onActivated();    // polymorphic hook
        logEvent("ACTIVATED");
    }

    void deactivate() {
        is_active_ = false;
        onDeactivated();  // polymorphic hook
        logEvent("DEACTIVATED");
    }

    // Common getters
    const std::string& getId()      const { return entity_id_; }
    const std::string& getUserId()  const { return user_id_; }
    const std::string& getTitle()   const { return title_; }
    const std::string& getContent() const { return content_; }
    bool               isActive()   const { return is_active_; }

    void setContent(const std::string& c) { content_ = c; }

    // Virtual hooks (optional override — Polymorphism)
    virtual void onActivated()   { std::cout << "[INFO] " << title_ << " activated\n"; }
    virtual void onDeactivated() { std::cout << "[INFO] " << title_ << " deactivated\n"; }

private:
    void logEvent(const std::string& event) {
        std::cout << "[LOG] " << entity_id_ << " :: " << event << "\n";
    }
};

// ================================================================
//  SECTION 4: WORKOUT PLAN — Inherits from FitnessEntity
// ================================================================

struct Exercise {
    std::string name;
    int         sets;
    int         reps;
    int         rest_seconds;
    std::string notes;

    Exercise(const std::string& n, int s, int r, int rest = 60,
             const std::string& notes = "")
        : name(n), sets(s), reps(r), rest_seconds(rest), notes(notes) {}

    std::string toString() const {
        return name + " — " + std::to_string(sets) + "x" + std::to_string(reps) +
               " | Rest: " + std::to_string(rest_seconds) + "s";
    }
};

class WorkoutPlan : public FitnessEntity {
private:
    WorkoutType     workout_type_;
    DifficultyLevel difficulty_;
    int             duration_min_;
    std::string     equipment_;
    std::vector<Exercise> exercises_;
    std::vector<Exercise> warmup_;
    std::vector<Exercise> cooldown_;
    bool            ai_generated_;

public:
    WorkoutPlan(const std::string& pid, const std::string& uid,
                const std::string& title, WorkoutType wtype,
                DifficultyLevel diff, int duration, const std::string& eq)
        : FitnessEntity(pid, uid, title),
          workout_type_(wtype), difficulty_(diff),
          duration_min_(duration), equipment_(eq), ai_generated_(false) {}

    // Implement pure virtual methods
    PlanType getType() const override { return PlanType::WORKOUT; }

    std::string getSummary() const override {
        std::string diff_str;
        switch (difficulty_) {
            case DifficultyLevel::BEGINNER:     diff_str = "Beginner"; break;
            case DifficultyLevel::INTERMEDIATE: diff_str = "Intermediate"; break;
            case DifficultyLevel::ADVANCED:     diff_str = "Advanced"; break;
            case DifficultyLevel::ELITE:        diff_str = "Elite"; break;
        }
        return "[WORKOUT] " + title_ + " | " + diff_str +
               " | " + std::to_string(duration_min_) + " min | " +
               std::to_string(exercises_.size()) + " exercises";
    }

    void validate() const override {
        if (title_.empty())
            throw std::runtime_error("Workout plan must have a title");
        if (duration_min_ < 10 || duration_min_ > 300)
            throw std::runtime_error("Duration must be 10–300 minutes");
    }

    double getScore() const override {
        // Score based on exercise count and duration
        double score = std::min(exercises_.size() * 10.0, 60.0);
        score += std::min(duration_min_ * 0.5, 40.0);
        return std::min(score, 100.0);
    }

    // Workout-specific methods
    void addExercise(const Exercise& ex) { exercises_.push_back(ex); }
    void addWarmup(const Exercise& ex)   { warmup_.push_back(ex); }
    void addCooldown(const Exercise& ex) { cooldown_.push_back(ex); }
    void setAIGenerated(bool val)        { ai_generated_ = val; }

    const std::vector<Exercise>& getExercises() const { return exercises_; }
    int getTotalSets() const {
        return std::accumulate(exercises_.begin(), exercises_.end(), 0,
                               [](int sum, const Exercise& e) { return sum + e.sets; });
    }

    void onActivated() override {
        std::cout << "[WORKOUT] Plan '" << title_ << "' is now your active training plan\n";
    }

    void printPlan() const {
        std::cout << "\n════════ WORKOUT: " << title_ << " ════════\n";
        std::cout << "Warm-Up:\n";
        for (const auto& e : warmup_)
            std::cout << "  • " << e.toString() << "\n";
        std::cout << "Main Workout:\n";
        int n = 1;
        for (const auto& e : exercises_)
            std::cout << "  " << n++ << ". " << e.toString() << "\n";
        std::cout << "Cool-Down:\n";
        for (const auto& e : cooldown_)
            std::cout << "  • " << e.toString() << "\n";
        std::cout << "Total Sets: " << getTotalSets() << "\n";
    }
};

// ================================================================
//  SECTION 5: DIET PLAN — Inherits from FitnessEntity
// ================================================================

struct Meal {
    std::string name;
    std::string time;
    std::string foods;
    Macros      macros;
    int         calories;

    Meal(const std::string& n, const std::string& t, const std::string& f,
         Macros m, int cal)
        : name(n), time(t), foods(f), macros(m), calories(cal) {}
};

class DietPlan : public FitnessEntity {
private:
    std::string        diet_type_;
    std::string        cuisine_;
    int                meals_per_day_;
    int                daily_calories_;
    Macros             daily_macros_;
    std::vector<Meal>  meals_;
    std::string        restrictions_;
    std::string        budget_;

public:
    DietPlan(const std::string& pid, const std::string& uid,
             const std::string& title, const std::string& dtype,
             const std::string& cuisine, int meals_per_day,
             int daily_cal, Macros macros)
        : FitnessEntity(pid, uid, title),
          diet_type_(dtype), cuisine_(cuisine),
          meals_per_day_(meals_per_day), daily_calories_(daily_cal),
          daily_macros_(macros) {}

    // Implement pure virtual methods
    PlanType getType() const override { return PlanType::DIET; }

    std::string getSummary() const override {
        return "[DIET] " + title_ + " | " + diet_type_ +
               " | " + std::to_string(daily_calories_) + " kcal/day | " +
               std::to_string(meals_per_day_) + " meals";
    }

    void validate() const override {
        if (daily_calories_ < 800 || daily_calories_ > 10000)
            throw std::runtime_error("Daily calories must be 800–10000 kcal");
        if (meals_per_day_ < 1 || meals_per_day_ > 10)
            throw std::runtime_error("Meals per day must be 1–10");
    }

    double getScore() const override {
        // Balanced macros = higher score
        double p = daily_macros_.protein() * 4.0 / daily_calories_;
        return std::min(p * 200.0, 100.0);
    }

    void addMeal(const Meal& meal) { meals_.push_back(meal); }
    void setRestrictions(const std::string& r) { restrictions_ = r; }
    void setBudget(const std::string& b) { budget_ = b; }

    int getDailyCalories() const { return daily_calories_; }
    const Macros& getDailyMacros() const { return daily_macros_; }
    const std::vector<Meal>& getMeals() const { return meals_; }

    void onActivated() override {
        std::cout << "[DIET] Plan '" << title_ << "' is now your active nutrition plan\n";
    }

    Macros computeActualMacros() const {
        return std::accumulate(meals_.begin(), meals_.end(),
                               Macros(0, 0, 0),
                               [](Macros sum, const Meal& m) { return sum + m.macros; });
    }
};

// ================================================================
//  SECTION 6: PROGRESS LOG — Builder Pattern
// ================================================================

class ProgressLog {
private:
    std::string log_id_;
    std::string user_id_;
    std::time_t log_date_;
    std::optional<double> weight_kg_;
    std::optional<int>    calories_consumed_;
    std::optional<int>    steps_;
    std::optional<int>    water_ml_;
    std::optional<double> sleep_hours_;
    bool                  workout_completed_;
    std::string           notes_;

public:
    ProgressLog(const std::string& lid, const std::string& uid)
        : log_id_(lid), user_id_(uid),
          log_date_(std::time(nullptr)), workout_completed_(false) {}

    // Fluent Builder interface (method chaining)
    ProgressLog& withWeight(double w)    { weight_kg_ = w; return *this; }
    ProgressLog& withCalories(int c)     { calories_consumed_ = c; return *this; }
    ProgressLog& withSteps(int s)        { steps_ = s; return *this; }
    ProgressLog& withWater(int ml)       { water_ml_ = ml; return *this; }
    ProgressLog& withSleep(double h)     { sleep_hours_ = h; return *this; }
    ProgressLog& withWorkout(bool done)  { workout_completed_ = done; return *this; }
    ProgressLog& withNotes(const std::string& n) { notes_ = n; return *this; }

    // Getters
    const std::string& getId()   const { return log_id_; }
    const std::string& getUserId() const { return user_id_; }
    bool workoutDone()            const { return workout_completed_; }
    std::optional<double> getWeight() const { return weight_kg_; }
    std::optional<double> getSleep()  const { return sleep_hours_; }

    // Wellness score calculation
    double wellnessScore() const {
        double score = 0.0, max = 0.0;
        if (workout_completed_) { score += 30; } max += 30;
        if (steps_) {
            score += std::min(20.0, *steps_ / 500.0);
        } max += 20;
        if (water_ml_) {
            score += (*water_ml_ >= 2000) ? 20.0 : (*water_ml_ / 100.0);
        } max += 20;
        if (sleep_hours_) {
            double s = *sleep_hours_;
            if (s >= 7 && s <= 9) score += 30;
            else score += std::max(0.0, 30.0 - std::abs(s - 8.0) * 6.0);
        } max += 30;
        return max > 0 ? (score / max) * 100.0 : 0.0;
    }

    void print() const {
        std::cout << "[LOG " << log_id_ << "] ";
        if (weight_kg_)         std::cout << "Weight: " << *weight_kg_ << "kg | ";
        if (workout_completed_) std::cout << "Workout ✓ | ";
        if (steps_)             std::cout << "Steps: " << *steps_ << " | ";
        if (water_ml_)          std::cout << "Water: " << *water_ml_ << "ml | ";
        if (sleep_hours_)       std::cout << "Sleep: " << *sleep_hours_ << "h | ";
        std::cout << "Wellness: " << wellnessScore() << "/100\n";
    }
};

// ================================================================
//  SECTION 7: OBSERVER PATTERN — Fitness Event Notifications
// ================================================================

struct FitnessEvent {
    std::string type;
    std::string user_id;
    std::string data;
    std::time_t timestamp;
    FitnessEvent(const std::string& t, const std::string& uid, const std::string& d)
        : type(t), user_id(uid), data(d), timestamp(std::time(nullptr)) {}
};

// Abstract observer interface
class IFitnessObserver {
public:
    virtual ~IFitnessObserver() = default;
    virtual void onEvent(const FitnessEvent& event) = 0;
    virtual std::string getName() const = 0;
};

// Concrete observer: Weight milestone tracker
class WeightMilestoneObserver : public IFitnessObserver {
public:
    void onEvent(const FitnessEvent& event) override {
        if (event.type == "WEIGHT_LOG") {
            std::cout << "[🏆 MILESTONE] Weight milestone tracking event for user "
                      << event.user_id << " — " << event.data << "\n";
        }
    }
    std::string getName() const override { return "WeightMilestoneObserver"; }
};

// Concrete observer: Workout streak tracker
class StreakObserver : public IFitnessObserver {
private:
    int current_streak_ = 0;
public:
    void onEvent(const FitnessEvent& event) override {
        if (event.type == "WORKOUT_COMPLETE") {
            current_streak_++;
            std::cout << "[🔥 STREAK] User " << event.user_id
                      << " — Current streak: " << current_streak_ << " days!\n";
            if (current_streak_ % 7 == 0)
                std::cout << "[🏆 ACHIEVEMENT] " << current_streak_
                          << "-day streak unlocked!\n";
        } else if (event.type == "NO_WORKOUT") {
            current_streak_ = 0;
        }
    }
    std::string getName() const override { return "StreakObserver"; }
};

// Concrete observer: Neural score updater
class NeuralScoreObserver : public IFitnessObserver {
public:
    void onEvent(const FitnessEvent& event) override {
        if (event.type == "PROGRESS_LOG") {
            std::cout << "[🧠 NEURAL] Recalculating neural fitness score for user "
                      << event.user_id << "\n";
        }
    }
    std::string getName() const override { return "NeuralScoreObserver"; }
};

// Event dispatcher (Subject)
class FitnessEventBus {
private:
    std::vector<std::shared_ptr<IFitnessObserver>> observers_;
    static FitnessEventBus* instance_;
    static std::mutex mutex_;

    FitnessEventBus() = default;
public:
    static FitnessEventBus& getInstance() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!instance_) instance_ = new FitnessEventBus();
        return *instance_;
    }

    void subscribe(std::shared_ptr<IFitnessObserver> obs) {
        observers_.push_back(obs);
        std::cout << "[BUS] Subscribed: " << obs->getName() << "\n";
    }

    void publish(const FitnessEvent& event) {
        for (auto& obs : observers_) obs->onEvent(event);
    }
};
FitnessEventBus* FitnessEventBus::instance_ = nullptr;
std::mutex FitnessEventBus::mutex_;

// ================================================================
//  SECTION 8: STRATEGY PATTERN — AI Provider abstraction
// ================================================================

class IAIStrategy {
public:
    virtual ~IAIStrategy() = default;
    virtual std::string generateWorkoutPlan(const User& user,
                                            const std::string& type,
                                            const std::string& level,
                                            int duration,
                                            const std::string& equipment) const = 0;
    virtual std::string generateMealPlan(const User& user,
                                         const std::string& diet_type,
                                         int meals_per_day,
                                         const std::string& restrictions) const = 0;
    virtual std::string chat(const std::string& message,
                              const std::string& context) const = 0;
    virtual std::string getProviderName() const = 0;
};

// Groq AI Strategy (free, Llama-powered)
class GroqAIStrategy : public IAIStrategy {
private:
    std::string api_key_;
    std::string base_url_ = "https://api.groq.com/openai/v1/chat/completions";
    std::string model_    = "llama-3.3-70b-versatile";

    std::string httpPost(const std::string& payload) const {
        // In production: use cpp-httplib or libcurl
        // httplib::Client cli("api.groq.com");
        // cli.set_default_headers({{"Authorization","Bearer " + api_key_}});
        // auto res = cli.Post("/openai/v1/chat/completions", payload, "application/json");
        // return res->body;
        std::cout << "[GROQ] Sending request to " << base_url_ << "\n";
        return R"({"choices":[{"message":{"content":"AI response would appear here."}}]})";
    }

public:
    explicit GroqAIStrategy(const std::string& key) : api_key_(key) {}

    std::string generateWorkoutPlan(const User& user, const std::string& type,
                                     const std::string& level, int duration,
                                     const std::string& equipment) const override {
        std::string prompt = "Create a " + level + " " + type + " workout for "
                           + std::to_string(duration) + " min using " + equipment
                           + ". User: " + std::to_string(user.getAge()) + "yo, "
                           + std::to_string(user.getWeight()) + "kg.";
        std::cout << "[AI-WORKOUT] Generating plan: " << prompt << "\n";
        return httpPost(prompt);
    }

    std::string generateMealPlan(const User& user, const std::string& diet_type,
                                   int meals_per_day, const std::string& restrictions) const override {
        double cal = user.calcAdjustedCalories();
        std::string prompt = "Create a " + diet_type + " meal plan with "
                           + std::to_string(meals_per_day) + " meals, "
                           + std::to_string((int)cal) + " kcal target."
                           + (restrictions.empty() ? "" : " Restrictions: " + restrictions);
        std::cout << "[AI-DIET] Generating plan: " << prompt << "\n";
        return httpPost(prompt);
    }

    std::string chat(const std::string& message, const std::string& context) const override {
        std::string prompt = context + "\nUser: " + message;
        std::cout << "[AI-CHAT] Processing: " << message << "\n";
        return httpPost(prompt);
    }

    std::string getProviderName() const override { return "Groq (Llama-3.3-70B)"; }
};

// ================================================================
//  SECTION 9: TEMPLATE CLASS — Generic repository
// ================================================================

template<typename T>
class DataRepository {
private:
    std::vector<T> data_;
    std::string    entity_name_;

public:
    explicit DataRepository(const std::string& name) : entity_name_(name) {}

    void add(const T& item) {
        data_.push_back(item);
        std::cout << "[REPO] Added to " << entity_name_ << " (total: " << data_.size() << ")\n";
    }

    std::optional<T> findById(const std::string& id) const {
        for (const auto& item : data_)
            if (item.getId() == id) return item;
        return std::nullopt;
    }

    std::vector<T> findByUserId(const std::string& uid) const {
        std::vector<T> results;
        std::copy_if(data_.begin(), data_.end(), std::back_inserter(results),
                     [&uid](const T& item) { return item.getUserId() == uid; });
        return results;
    }

    size_t count() const { return data_.size(); }
    void clear() { data_.clear(); }
};

// ================================================================
//  SECTION 10: FACTORY PATTERN — Plan creation
// ================================================================

class PlanFactory {
public:
    static std::unique_ptr<WorkoutPlan> createWorkoutPlan(
        const std::string& pid, const std::string& uid,
        const std::string& title, WorkoutType type,
        DifficultyLevel diff, int duration, const std::string& eq)
    {
        auto plan = std::make_unique<WorkoutPlan>(pid, uid, title, type, diff, duration, eq);

        // Add default exercises based on type
        if (type == WorkoutType::FULL_BODY) {
            plan->addWarmup(Exercise("Jump Rope", 1, 3, 30, "3 minutes cardio warm-up"));
            plan->addExercise(Exercise("Barbell Squat", 4, 8, 90, "Keep chest up, knees tracking toes"));
            plan->addExercise(Exercise("Bench Press", 4, 10, 90, "Full range of motion, control eccentric"));
            plan->addExercise(Exercise("Bent-Over Row", 3, 10, 75, "Squeeze shoulder blades at top"));
            plan->addExercise(Exercise("Overhead Press", 3, 10, 75, "Engage core throughout"));
            plan->addExercise(Exercise("Romanian Deadlift", 3, 12, 75, "Hinge at hips, slight knee bend"));
            plan->addCooldown(Exercise("Full Body Stretch", 1, 5, 0, "Hold each stretch 30 seconds"));
        } else if (type == WorkoutType::UPPER_BODY) {
            plan->addWarmup(Exercise("Arm Circles + Band Pull-Aparts", 1, 2, 30, "2 sets each"));
            plan->addExercise(Exercise("Incline Dumbbell Press", 4, 10, 90, "45° incline, full stretch"));
            plan->addExercise(Exercise("Pull-Ups", 4, 8, 90, "Full dead hang to chin over bar"));
            plan->addExercise(Exercise("Lateral Raises", 3, 15, 60, "Control the negative"));
            plan->addExercise(Exercise("Cable Row", 3, 12, 75, "Drive elbows back, not up"));
            plan->addExercise(Exercise("Tricep Dips", 3, 12, 60, "Lean forward slightly"));
            plan->addExercise(Exercise("Hammer Curls", 3, 12, 60, "Neutral grip, no swinging"));
            plan->addCooldown(Exercise("Chest + Shoulder Stretch", 1, 3, 0, "Doorway stretch"));
        }

        plan->validate();
        std::cout << "[FACTORY] Created: " << plan->getSummary() << "\n";
        return plan;
    }

    static std::unique_ptr<DietPlan> createDietPlan(
        const std::string& pid, const std::string& uid,
        const std::string& title, const std::string& dtype,
        const std::string& cuisine, int meals, const User& user)
    {
        int cal = (int)user.calcAdjustedCalories();
        Macros m = user.calcMacros();
        auto plan = std::make_unique<DietPlan>(pid, uid, title, dtype, cuisine, meals, cal, m);

        // Sample meals
        int mealCal = cal / meals;
        Macros mMacros(m.protein()/meals, m.carbs()/meals, m.fat()/meals);

        plan->addMeal(Meal("Breakfast", "07:00", "Oats + eggs + banana", mMacros, mealCal));
        plan->addMeal(Meal("Lunch", "13:00", "Chicken rice + vegetables", mMacros, mealCal));
        plan->addMeal(Meal("Dinner", "19:00", "Salmon + sweet potato + broccoli", mMacros, mealCal));

        plan->validate();
        std::cout << "[FACTORY] Created: " << plan->getSummary() << "\n";
        return plan;
    }
};

// ================================================================
//  SECTION 11: SINGLETON — Database Manager
// ================================================================

class DatabaseManager {
private:
    bool        connected_;
    std::string host_;
    std::string db_name_;
    static DatabaseManager* instance_;
    static std::mutex       mutex_;

    DatabaseManager() : connected_(false) {}
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

public:
    static DatabaseManager& getInstance() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!instance_) instance_ = new DatabaseManager();
        return *instance_;
    }

    bool connect(const std::string& host, const std::string& db,
                  const std::string& user, const std::string& pass) {
        // In production: pqxx::connection C("host="+host+" dbname="+db+" user="+user+" password="+pass);
        std::cout << "[DB] Connecting to " << db << "@" << host << "...\n";
        host_ = host; db_name_ = db;
        connected_ = true;
        std::cout << "[DB] Connected successfully!\n";
        return true;
    }

    bool isConnected() const { return connected_; }

    // CRUD operations (production: use libpqxx prepared statements)
    bool insertUser(const User& u) {
        if (!connected_) throw std::runtime_error("Database not connected");
        std::cout << "[DB] INSERT fitness_users: " << u.getName() << "\n";
        return true;
    }

    bool insertWorkoutPlan(const WorkoutPlan& plan) {
        std::cout << "[DB] INSERT workout_plans: " << plan.getTitle() << "\n";
        return true;
    }

    bool insertProgressLog(const ProgressLog& log) {
        std::cout << "[DB] INSERT progress_logs: " << log.getId() << "\n";
        return true;
    }

    bool updateUserWeight(const std::string& uid, double new_weight) {
        std::cout << "[DB] UPDATE fitness_users SET weight_kg=" << new_weight
                  << " WHERE id=" << uid << "\n";
        return true;
    }
};
DatabaseManager* DatabaseManager::instance_ = nullptr;
std::mutex DatabaseManager::mutex_;

// ================================================================
//  SECTION 12: COMMAND PATTERN — API request handlers
// ================================================================

class ICommand {
public:
    virtual ~ICommand() = default;
    virtual std::string execute(const std::string& body) = 0;
    virtual std::string getRoute() const = 0;
};

class CreateUserCommand : public ICommand {
private:
    DatabaseManager& db_;
public:
    explicit CreateUserCommand(DatabaseManager& db) : db_(db) {}

    std::string execute(const std::string& body) override {
        // Parse JSON body and create user
        std::cout << "[CMD] CreateUser: " << body << "\n";
        // User u = User::fromJson(body); db_.insertUser(u);
        return R"({"success":true,"message":"User created","user_id":"new-uuid"})";
    }

    std::string getRoute() const override { return "POST /api/users"; }
};

class GenerateWorkoutCommand : public ICommand {
private:
    DatabaseManager&  db_;
    IAIStrategy&      ai_;
    FitnessEventBus&  bus_;
public:
    GenerateWorkoutCommand(DatabaseManager& db, IAIStrategy& ai, FitnessEventBus& bus)
        : db_(db), ai_(ai), bus_(bus) {}

    std::string execute(const std::string& body) override {
        std::cout << "[CMD] GenerateWorkout\n";
        // Parse params, load user, generate via AI, save to DB
        bus_.publish(FitnessEvent("WORKOUT_GENERATE", "user-id", body));
        return R"({"success":true,"plan_id":"plan-uuid","content":"..."})";
    }

    std::string getRoute() const override { return "POST /api/workouts/generate"; }
};

class LogProgressCommand : public ICommand {
private:
    DatabaseManager& db_;
    FitnessEventBus& bus_;
public:
    LogProgressCommand(DatabaseManager& db, FitnessEventBus& bus)
        : db_(db), bus_(bus) {}

    std::string execute(const std::string& body) override {
        std::cout << "[CMD] LogProgress\n";
        ProgressLog log("log-uuid", "user-uuid");
        log.withWeight(70.5).withWorkout(true).withSteps(9000).withSleep(7.5);
        db_.insertProgressLog(log);
        bus_.publish(FitnessEvent("PROGRESS_LOG", "user-uuid", body));
        bus_.publish(FitnessEvent("WORKOUT_COMPLETE", "user-uuid", ""));
        return R"({"success":true,"wellness_score":)" + std::to_string((int)log.wellnessScore()) + "}";
    }

    std::string getRoute() const override { return "POST /api/progress"; }
};

// ================================================================
//  SECTION 13: API ROUTER — Routes requests to commands
// ================================================================

class APIRouter {
private:
    std::unordered_map<std::string, std::unique_ptr<ICommand>> routes_;

public:
    void registerCommand(std::unique_ptr<ICommand> cmd) {
        std::string route = cmd->getRoute();
        std::cout << "[ROUTER] Registered: " << route << "\n";
        routes_[route] = std::move(cmd);
    }

    std::string dispatch(const std::string& route, const std::string& body) {
        auto it = routes_.find(route);
        if (it == routes_.end())
            return R"({"error":"Route not found","code":404})";
        return it->second->execute(body);
    }

    void listRoutes() const {
        std::cout << "\n══ Registered API Routes ══\n";
        for (const auto& [route, _] : routes_)
            std::cout << "  " << route << "\n";
        std::cout << "\n";
    }
};

} // namespace NeuroFit

// ================================================================
//  MAIN — Server initialization and demo
// ================================================================

int main() {
    using namespace NeuroFit;

    std::cout << R"(
  ┌─────────────────────────────────────────────┐
  │     NeuroFit AI — C++ Backend Server        │
  │     Full OOPS Architecture v2.0             │
  │     Port: 8080                              │
  └─────────────────────────────────────────────┘
)" << "\n";

    // 1. Database Singleton
    auto& db = DatabaseManager::getInstance();
    db.connect("db.pliyvcazofacadrdyywh.supabase.co",
               "postgres", "postgres", "YOUR_DB_PASSWORD");

    // 2. Event Bus (Observer Pattern)
    auto& bus = FitnessEventBus::getInstance();
    bus.subscribe(std::make_shared<WeightMilestoneObserver>());
    bus.subscribe(std::make_shared<StreakObserver>());
    bus.subscribe(std::make_shared<NeuralScoreObserver>());

    // 3. AI Strategy (Strategy Pattern)
    GroqAIStrategy aiStrategy("YOUR_GROQ_API_KEY");
    std::cout << "\n[AI] Using provider: " << aiStrategy.getProviderName() << "\n\n";

    // 4. API Router (Command Pattern)
    APIRouter router;
    router.registerCommand(std::make_unique<CreateUserCommand>(db));
    router.registerCommand(std::make_unique<GenerateWorkoutCommand>(db, aiStrategy, bus));
    router.registerCommand(std::make_unique<LogProgressCommand>(db, bus));
    router.listRoutes();

    // ── DEMO: Showcase all OOPS concepts ──

    // Demo 1: User class (Encapsulation)
    std::cout << "══ DEMO 1: User Encapsulation ══\n";
    User ravi("u001", "Ravi Kumar", 22, 72.0, 175.0,
              FitnessGoal::MUSCLE_GAIN, ActivityLevel::MODERATE, Gender::MALE);
    ravi.print();

    // Demo 2: Factory Pattern
    std::cout << "\n══ DEMO 2: Factory Pattern ══\n";
    auto workout = PlanFactory::createWorkoutPlan(
        "p001", "u001", "Full Body Strength", WorkoutType::FULL_BODY,
        DifficultyLevel::INTERMEDIATE, 60, "Full Gym");
    workout->printPlan();

    auto diet = PlanFactory::createDietPlan(
        "p002", "u001", "High Protein Plan", "High Protein",
        "Indian", 5, ravi);

    // Demo 3: Polymorphism
    std::cout << "\n══ DEMO 3: Polymorphism ══\n";
    std::vector<FitnessEntity*> entities = { workout.get(), diet.get() };
    for (auto* e : entities)
        std::cout << "  [" << (e->getType()==PlanType::WORKOUT?"WORKOUT":"DIET") << "] "
                  << e->getSummary() << " | Score: " << (int)e->getScore() << "\n";

    // Demo 4: Observer Pattern
    std::cout << "\n══ DEMO 4: Observer Pattern ══\n";
    bus.publish(FitnessEvent("WORKOUT_COMPLETE", "u001", "Full body done"));
    bus.publish(FitnessEvent("WEIGHT_LOG",       "u001", "70.5 kg"));
    bus.publish(FitnessEvent("PROGRESS_LOG",     "u001", "Day 1 complete"));

    // Demo 5: Builder Pattern
    std::cout << "\n══ DEMO 5: Builder Pattern ══\n";
    ProgressLog log("l001", "u001");
    log.withWeight(72.0).withWorkout(true).withSteps(9500)
       .withWater(2800).withSleep(7.5).withNotes("Felt strong today");
    log.print();

    // Demo 6: Template class
    std::cout << "\n══ DEMO 6: Template Repository ══\n";
    DataRepository<ProgressLog> logRepo("progress_logs");
    logRepo.add(log);
    std::cout << "Repository count: " << logRepo.count() << "\n";

    // Demo 7: AI Strategy
    std::cout << "\n══ DEMO 7: AI Strategy ══\n";
    aiStrategy.generateWorkoutPlan(ravi, "Full Body", "Intermediate", 60, "Full Gym");
    aiStrategy.generateMealPlan(ravi, "High Protein", 5, "none");

    // Demo 8: Command Pattern — API dispatch
    std::cout << "\n══ DEMO 8: API Router (Command Pattern) ══\n";
    std::string r1 = router.dispatch("POST /api/users", R"({"name":"Ravi","age":22})");
    std::cout << "Response: " << r1 << "\n";
    std::string r2 = router.dispatch("POST /api/progress", R"({"weight":72.0,"workout":true})");
    std::cout << "Response: " << r2 << "\n";

    // 5. HTTP Server (production)
    std::cout << "\n[SERVER] NeuroFit AI backend ready on port 8080\n";
    /*
    httplib::Server svr;
    svr.Options(".*", [](const auto& req, auto& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.status = 204;
    });
    svr.Post("/api/users",             [&](const auto& req, auto& res) {
        res.set_content(router.dispatch("POST /api/users", req.body), "application/json");
        res.set_header("Access-Control-Allow-Origin", "*");
    });
    svr.Post("/api/workouts/generate", [&](const auto& req, auto& res) {
        res.set_content(router.dispatch("POST /api/workouts/generate", req.body), "application/json");
        res.set_header("Access-Control-Allow-Origin", "*");
    });
    svr.Post("/api/progress",          [&](const auto& req, auto& res) {
        res.set_content(router.dispatch("POST /api/progress", req.body), "application/json");
        res.set_header("Access-Control-Allow-Origin", "*");
    });
    svr.listen("0.0.0.0", 8080);
    */

    return 0;
}

/*
 * ================================================================
 *  SQL SCHEMA — Supabase Tables
 * ================================================================
 *
 * CREATE TABLE fitness_users (
 *   id             UUID PRIMARY KEY DEFAULT gen_random_uuid(),
 *   name           TEXT NOT NULL,
 *   age            INTEGER CHECK(age BETWEEN 10 AND 120),
 *   weight_kg      NUMERIC(5,2) CHECK(weight_kg > 0),
 *   height_cm      NUMERIC(5,2) CHECK(height_cm > 0),
 *   gender         TEXT DEFAULT 'male',
 *   fitness_goal   TEXT NOT NULL,
 *   activity_level TEXT NOT NULL,
 *   created_at     TIMESTAMPTZ DEFAULT now(),
 *   updated_at     TIMESTAMPTZ DEFAULT now()
 * );
 *
 * CREATE TABLE workout_plans (
 *   id          UUID PRIMARY KEY DEFAULT gen_random_uuid(),
 *   user_id     UUID REFERENCES fitness_users(id) ON DELETE CASCADE,
 *   title       TEXT NOT NULL,
 *   plan_data   JSONB,
 *   is_active   BOOLEAN DEFAULT true,
 *   created_at  TIMESTAMPTZ DEFAULT now()
 * );
 *
 * CREATE TABLE diet_plans (
 *   id              UUID PRIMARY KEY DEFAULT gen_random_uuid(),
 *   user_id         UUID REFERENCES fitness_users(id) ON DELETE CASCADE,
 *   title           TEXT NOT NULL,
 *   plan_data       JSONB,
 *   daily_calories  INTEGER,
 *   is_active       BOOLEAN DEFAULT true,
 *   created_at      TIMESTAMPTZ DEFAULT now()
 * );
 *
 * CREATE TABLE progress_logs (
 *   id                UUID PRIMARY KEY DEFAULT gen_random_uuid(),
 *   user_id           UUID REFERENCES fitness_users(id) ON DELETE CASCADE,
 *   log_date          DATE DEFAULT CURRENT_DATE,
 *   weight_kg         NUMERIC(5,2),
 *   calories_consumed INTEGER,
 *   steps             INTEGER,
 *   water_ml          INTEGER,
 *   sleep_hours       NUMERIC(4,2),
 *   workout_completed BOOLEAN DEFAULT false,
 *   notes             TEXT,
 *   created_at        TIMESTAMPTZ DEFAULT now()
 * );
 *
 * CREATE TABLE body_metrics (
 *   id         UUID PRIMARY KEY DEFAULT gen_random_uuid(),
 *   user_id    UUID REFERENCES fitness_users(id) ON DELETE CASCADE,
 *   chest      NUMERIC(5,2),
 *   waist      NUMERIC(5,2),
 *   hips       NUMERIC(5,2),
 *   arms       NUMERIC(5,2),
 *   thighs     NUMERIC(5,2),
 *   calves     NUMERIC(5,2),
 *   created_at TIMESTAMPTZ DEFAULT now()
 * );
 *
 * CREATE TABLE chat_history (
 *   id         UUID PRIMARY KEY DEFAULT gen_random_uuid(),
 *   user_id    UUID REFERENCES fitness_users(id) ON DELETE CASCADE,
 *   role       TEXT CHECK(role IN ('user','assistant')),
 *   content    TEXT NOT NULL,
 *   created_at TIMESTAMPTZ DEFAULT now()
 * );
 *
 * ================================================================
 *  OOPS CONCEPTS SUMMARY:
 *
 *  1. ABSTRACTION      → FitnessEntity (pure virtual: getType, getSummary,
 *                          validate, getScore), IAIStrategy, IFitnessObserver,
 *                          ICommand
 *  2. ENCAPSULATION    → User (private members + validation in setters)
 *  3. INHERITANCE      → WorkoutPlan : FitnessEntity
 *                         DietPlan   : FitnessEntity
 *                         WeightMilestoneObserver : IFitnessObserver
 *                         StreakObserver          : IFitnessObserver
 *                         GroqAIStrategy          : IAIStrategy
 *  4. POLYMORPHISM     → FitnessEntity* pointers to WorkoutPlan & DietPlan
 *                         Virtual dispatch for getType(), getSummary()
 *  5. TEMPLATES        → DataRepository<T> generic container
 *  6. SINGLETON        → DatabaseManager, FitnessEventBus
 *  7. FACTORY          → PlanFactory::createWorkoutPlan/createDietPlan
 *  8. OBSERVER         → FitnessEventBus + IFitnessObserver implementations
 *  9. STRATEGY         → IAIStrategy + GroqAIStrategy
 * 10. BUILDER          → ProgressLog::with*() fluent interface
 * 11. COMMAND          → ICommand + CreateUserCommand + GenerateWorkoutCommand
 * ================================================================
 */
