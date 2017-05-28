#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <chrono>
#include <vector>
#include <cstdint>

using namespace std;

class bf_interpreter {
	struct instruction {
		enum class opcode {
			add_to_dp,
			add_to_cell,
			output,
			input,
			branch_if_zero,
			branch_if_nonzero,
			store,
			add_cells
		};

		opcode op;
		int data1;
		int data2;
	};

	unique_ptr<uint8_t[]> memory;
	unique_ptr<uint8_t[]> raw_program;
	vector<instruction> program;
	size_t raw_size;
	size_t ip;
	size_t dp;

	size_t find_run(size_t start_index);
	size_t find_matched(size_t start_index);

	public:
		bf_interpreter(string filename);

		void parse();
		void print(string filename);
		void run();
};

bf_interpreter::bf_interpreter(string filename) {
	ifstream file(filename, ios::ate | ios::binary);

	this->raw_size = static_cast<size_t>(file.tellg());
	this->ip = 0;
	this->dp = 0;

	this->raw_program = make_unique<uint8_t[]>(this->raw_size);
	this->memory = make_unique<uint8_t[]>(65536);

	file.seekg(ios::beg);
	file.read(reinterpret_cast<char*>(this->raw_program.get()), this->raw_size);
	file.close();
}

void bf_interpreter::print(string filename) {
	ofstream file(filename);

	for (auto& i : this->program) {
		switch (i.op) {
			case instruction::opcode::add_to_dp: file << "ADD_DP\t" << i.data1; break;
			case instruction::opcode::add_to_cell: file << "ADD_CL\t" << i.data1; break;
			case instruction::opcode::add_cells: file << "ADD\t" << i.data1 << "\t" << i.data2; break;
			case instruction::opcode::output: file << "OUT\t"; break;
			case instruction::opcode::input: file << "IN\t"; break;
			case instruction::opcode::store: file << "STORE\t" << i.data1; break;
			case instruction::opcode::branch_if_zero: file << "BZ\t" << i.data1; break;
			case instruction::opcode::branch_if_nonzero: file << "BNZ\t" << i.data1; break;
		}

		file << endl;
	}
}

size_t bf_interpreter::find_run(size_t start_index) {
	auto character = this->raw_program[start_index];
	size_t count = 0;

	while (start_index < this->raw_size && this->raw_program[start_index++] == character)
		count++;

	return count;
}

size_t bf_interpreter::find_matched(size_t start_index) {
	size_t needed = 1;
	auto direction = this->program[start_index].op == instruction::opcode::branch_if_zero ? 1 : -1;

	while (needed) {
		start_index += direction;

		if (this->program[start_index].op == instruction::opcode::branch_if_zero)
			needed += direction;
		else if (this->program[start_index].op == instruction::opcode::branch_if_nonzero)
			needed -= direction;
	}

	return start_index;
}

void bf_interpreter::parse() {
	for (size_t i = 0; i < this->raw_size; i++) {
		instruction instr;

		instr.data1 = 0;
		instr.data2 = 0;

		switch (this->raw_program[i]) {
			case '>':
				instr.op = instruction::opcode::add_to_dp;
				instr.data1 = this->find_run(i);

				i += instr.data1 - 1;

				break;

			case '<':
				instr.op = instruction::opcode::add_to_dp;
				instr.data1 = -1 * this->find_run(i);

				i -= instr.data1 + 1;

				break;

			case '+':
				instr.op = instruction::opcode::add_to_cell;
				instr.data1 = this->find_run(i);

				i += instr.data1 - 1;

				break;

			case '-':
				instr.op = instruction::opcode::add_to_cell;
				instr.data1 = -1 * this->find_run(i);

				i -= instr.data1 + 1;

				break;

			case '.':
				instr.op = instruction::opcode::output;

				break;

			case ',':
				instr.op = instruction::opcode::input;

				break;

			case '[':
				instr.op = instruction::opcode::branch_if_zero;

				break;

			case ']':
				instr.op = instruction::opcode::branch_if_nonzero;

				break;

			default:
				continue;
		}

		this->program.push_back(instr);
	}

	for (size_t i = 0; i < this->program.size(); i++) {
		switch (this->program[i].op) {
			case instruction::opcode::branch_if_zero:
				if (i + 2 < this->program.size() && this->program[i + 1].op == instruction::opcode::add_to_cell && this->program[i + 1].data1 % 2 == 0 && this->program[i + 2].op == instruction::opcode::branch_if_nonzero) {
					this->program[i].op = instruction::opcode::store;
					this->program[i].data1 = 0;
					this->program.erase(this->program.begin() + i + 1, this->program.begin() + i + 3);

					continue;
				}
				else if (i + 5 < this->program.size() && this->program[i + 1].op == instruction::opcode::add_to_cell && this->program[i + 1].data1 == -1 && this->program[i + 5].op == instruction::opcode::branch_if_nonzero) {
					if (this->program[i + 2].op != instruction::opcode::add_to_dp || this->program[i + 4].op != instruction::opcode::add_to_dp || this->program[i + 3].op != instruction::opcode::add_to_cell)
						continue;
					if (this->program[i + 2].data1 < 0 && this->program[i + 4].data1 < 0)
						continue;
					if (this->program[i + 2].data1 >= 0 && this->program[i + 4].data1 >= 0)
						continue;

					this->program[i].op = instruction::opcode::add_cells;
					this->program[i].data1 = this->program[i + 2].data1;
					this->program[i].data2 = this->program[i + 3].data1;

					this->program[i + 1].op = instruction::opcode::store;
					this->program[i + 1].data1 = 0;

					this->program.erase(this->program.begin() + i + 2, this->program.begin() + i + 6);
				}

				break;
		}
	}

	for (size_t i = 0; i < this->program.size(); i++) {
		switch (this->program[i].op) {
			case instruction::opcode::branch_if_zero:
			case instruction::opcode::branch_if_nonzero:
				this->program[i].data1 = this->find_matched(i) + 1;

				break;
		}
	}
}

void bf_interpreter::run() {
	while (this->ip < this->program.size()) {
		auto instr = this->program[this->ip++];

		switch (instr.op) {
			case instruction::opcode::add_to_dp: this->dp += instr.data1; break;
			case instruction::opcode::add_to_cell: this->memory[this->dp] += static_cast<uint8_t>(instr.data1); break;
			case instruction::opcode::output: cout << this->memory[this->dp]; break;
			case instruction::opcode::input: cin >> this->memory[this->dp]; break;
			case instruction::opcode::store: this->memory[this->dp] = static_cast<uint8_t>(instr.data1); break;

			case instruction::opcode::add_cells:
				this->memory[this->dp + instr.data1] += static_cast<uint8_t>(instr.data2 * this->memory[this->dp]);

				break;

			case instruction::opcode::branch_if_zero:
				if (this->memory[this->dp] == 0)
					this->ip = instr.data1;

				break;

			case instruction::opcode::branch_if_nonzero:
				if (this->memory[this->dp] != 0)
					this->ip = instr.data1;

				break;
		}
	}
}

int main() {
	bf_interpreter bf("Hello World.txt");
	bf.parse();
	bf.print("Output.txt");

	auto start = chrono::system_clock::now();
	bf.run();
	auto end = chrono::system_clock::now();

	cout << endl << endl << "Total Time: " << chrono::duration_cast<chrono::milliseconds>(end - start).count() << endl;

	system("pause");

	return 0;
}
