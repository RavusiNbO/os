require 'monitor'
require 'thread'

# Класс, представляющий общую ванную комнату.
# Он использует Monitor для управления доступом и синхронизацией.
class SingleGenderBathroom
  include MonitorMixin

  MALE = 0
  FEMALE = 1
  GENDER_NAMES = { MALE => "Male", FEMALE => "Female" }

  def initialize
    super # Инициализация MonitorMixin
    
    @current_gender = nil  # Пол, который сейчас занимает ванную (MALE, FEMALE, или nil)
    @count = [0, 0]        # [count_male, count_female] - количество людей соответствующего пола внутри
    
    # Условные переменные для ожидания:
    # Ожидание освобождения ванной комнаты (когда @current_gender меняется)
    @other_gender_waiting = new_cond 
  end

  # Метод, который вызывает каждый студент для входа в ванную комнату.
  def enter(gender_id)
    gender_name = GENDER_NAMES[gender_id]

    synchronize do
      puts "[#{gender_name}] Attempting to enter..."

      # Основное условие блокировки:
      # Ждать, пока ванная занята другим полом.
      # (@current_gender != nil) && (@current_gender != gender_id)
      while @current_gender.nil? == false && @current_gender != gender_id
        puts "[#{gender_name}] Waiting for bathroom to be free of #{GENDER_NAMES[@current_gender]}"
        @other_gender_waiting.wait # Ожидаем сигнала от покидающего человека другого пола
      end
      
      # Как только условие соблюдено (ванная либо пуста, либо занята тем же полом):
      
      # Если студент, который заходит, является ПЕРВЫМ студентом этого пола,
      # мы "блокируем" ванную комнату для этого пола.
      if @count[gender_id] == 0
        @current_gender = gender_id
        puts "[#{gender_name}] FIRST IN. Room locked for their gender."
      end
      
      @count[gender_id] += 1
      puts "[#{gender_name}] Entered room. (Count: #{@count[gender_id]})"
    end

    # Время, проведенное в ванной комнате (эмулируется sleep)
    sleep(rand(1.0..2.0))
  end

  # Метод, который вызывает каждый студент для выхода из ванной комнаты.
  def leave(gender_id)
    gender_name = GENDER_NAMES[gender_id]
    
    synchronize do
      puts "<<< [#{gender_name}] Leaving room. (Count before: #{@count[gender_id]})"
      @count[gender_id] -= 1
      
      # Если студент, который выходит, является ПОСЛЕДНИМ студентом этого пола.
      if @count[gender_id] == 0
        puts "!!! [#{gender_name} LAST] Unlocking the door. Signalling others."
        @current_gender = nil # Освобождаем ванную комнату
        @other_gender_waiting.broadcast # Уведомляем всех ожидающих (другой пол может войти)
      end
    end
  end
end

# --- Основная логика симуляции ---

bathroom = SingleGenderBathroom.new

num_mens = ARGV[0].to_i > 0 ? ARGV[0].to_i : 5
num_womans = ARGV[1].to_i > 0 ? ARGV[1].to_i : (ARGV[0].to_i > 0 ? ARGV[0].to_i : 5)

total_students = num_mens + num_womans
threads = []

puts "Starting simulation with #{num_mens} Male and #{num_womans} Female students."

# Создание и запуск потоков
(0...num_mens).each do |i|
  threads << Thread.new do
    bathroom.enter(SingleGenderBathroom::MALE)
    bathroom.leave(SingleGenderBathroom::MALE)
  end
end

(0...num_womans).each do |i|
  threads << Thread.new do
    bathroom.enter(SingleGenderBathroom::FEMALE)
    bathroom.leave(SingleGenderBathroom::FEMALE)
  end
end

# Ожидание завершения всех потоков
threads.each(&:join)

puts "\nAll students have finished using the bathroom. Simulation complete."
