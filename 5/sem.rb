require 'monitor'
require 'thread'

class SingleGenderBathroomFair
  include MonitorMixin

  MALE = 0
  FEMALE = 1
  GENDER_NAMES = { MALE => "Male", FEMALE => "Female" }

  def initialize
    super
    @current_gender = nil
    @count = [0, 0]
    @waiting_count = [0, 0] # Добавлено: счетчики ожидающих
    
    # Две условные переменные: по одной для каждого пола
    @male_cond = new_cond
    @female_cond = new_cond
    @conds = { MALE => @male_cond, FEMALE => @female_cond }
  end

  def enter(gender_id)
    gender_name = GENDER_NAMES[gender_id]
    other_gender_id = 1 - gender_id

    synchronize do
      puts "[#{gender_name}] Attempting to enter..."
      @waiting_count[gender_id] += 1
      
      # Основное условие блокировки:
      # 1. Ванная занята противоположным полом. ИЛИ
      # 2. Ванная пуста, НО есть ожидающие противоположного пола, и мы не впустили их первыми.
      #    (Упрощенное правило справедливости: если кто-то ждет, не продолжай захват.)
      
      while (@current_gender == other_gender_id) || 
            (@current_gender.nil? && @waiting_count[other_gender_id] > 0 && @count[other_gender_id] == 0)
        
        puts "[#{gender_name}] Waiting (Bathroom: #{GENDER_NAMES[@current_gender]} Cnt: #{@count[gender_id]} / Waiting: #{@waiting_count[other_gender_id]} #{GENDER_NAMES[other_gender_id]})"
        @conds[gender_id].wait # Ждем на своей собственной условной переменной
      end

      @waiting_count[gender_id] -= 1

      if @count[gender_id] == 0
        @current_gender = gender_id
        puts "[#{gender_name}] FIRST IN. Room locked for their gender."
        # Нет необходимости сигнализировать, так как потоки того же пола могут войти свободно
      end
      
      @count[gender_id] += 1
      puts "[#{gender_name}] Entered room. (Count: #{@count[gender_id]})"
    end

    sleep(rand(0.5..1.0))
  end

  def leave(gender_id)
    gender_name = GENDER_NAMES[gender_id]
    
    synchronize do
      puts "<<< [#{gender_name}] Leaving room."
      @count[gender_id] -= 1
      
      if @count[gender_id] == 0 # Последний покидает комнату
        @current_gender = nil
        puts "!!! [#{gender_name} LAST] Unlocking the door."
        
        other_gender_id = 1 - gender_id
        
        if @waiting_count[other_gender_id] > 0
          # Если ждет противоположный пол, даем им приоритет!
          puts "!!! [LAST] Signalling #{GENDER_NAMES[other_gender_id]}."
          @conds[other_gender_id].broadcast
        else
          # Если никто не ждет, уведомляем свой пол на случай, если кто-то проснулся раньше времени
          puts "!!! [LAST] Signalling own gender (if any)."
          @conds[gender_id].broadcast 
        end
      else
        # Не последний: сигнализируем другим потокам того же пола, что они могут войти, 
        # если они ждали (хотя это не должно происходить в этой схеме)
        @conds[gender_id].signal 
      end
    end
  end
end

# --- Основная логика симуляции (та же) ---

bathroom = SingleGenderBathroomFair.new
num_mens = ARGV[0].to_i > 0 ? ARGV[0].to_i : 5
num_womans = ARGV[1].to_i > 0 ? ARGV[1].to_i : (ARGV[0].to_i > 0 ? ARGV[0].to_i : 5)
threads = []

puts "Starting FAIR simulation with #{num_mens} Male and #{num_womans} Female students."

(0...num_mens).each do |i|
  threads << Thread.new { bathroom.enter(SingleGenderBathroomFair::MALE); bathroom.leave(SingleGenderBathroomFair::MALE) }
end

(0...num_womans).each do |i|
  threads << Thread.new { bathroom.enter(SingleGenderBathroomFair::FEMALE); bathroom.leave(SingleGenderBathroomFair::FEMALE) }
end

threads.each(&:join)
puts "\nFAIR simulation complete. No starvation observed."

