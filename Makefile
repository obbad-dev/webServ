NAME := webServ

CXX := c++
CXXFLAGS := -Wall -Wextra -Werror -std=c++98

INCLUDES := -Isrc \
            -Isrc/Program \
            -Isrc/classes/parsing/location \
            -Isrc/classes/parsing/parseConfig \
            -Isrc/classes/parsing/server \
            -Isrc/classes/openConnection \
            -Isrc/classes/http \
            -Isrc/classes/helperFunc

SRCS := $(wildcard src/Program/*.cpp) \
        $(wildcard src/classes/parsing/location/*.cpp) \
        $(wildcard src/classes/parsing/parseConfig/*.cpp) \
        $(wildcard src/classes/parsing/server/*.cpp) \
        $(wildcard src/classes/openConnection/*.cpp) \
        $(wildcard src/classes/http/*.cpp) \
        $(wildcard src/classes/helperFunc/*.cpp)

OBJ_DIR := obj
OBJS := $(SRCS:%.cpp=$(OBJ_DIR)/%.o)

RM := rm -f

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@


$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	$(RM) -r $(OBJ_DIR)

fclean: clean
	$(RM) $(NAME)

re: fclean all

.PHONY: all clean fclean re