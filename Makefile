NAME := webServ

CXX := c++
CXXFLAGS := -Wall -Wextra -Werror -std=c++98

INCLUDES := -Isrc -Isrc/Program -Isrc/classes/location -Isrc/classes/parseConfig -Isrc/classes/server -Isrc/classes/openConnection

SRCS := $(wildcard src/Program/*.cpp src/classes/location/*.cpp src/classes/parseConfig/*.cpp src/classes/server/*.cpp src/classes/openConnection/*.cpp)

OBJ_DIR := obj
OBJS := $(SRCS:%.cpp=$(OBJ_DIR)/%.o)

RM := rm -f

.PHONY: all clean fclean re

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

