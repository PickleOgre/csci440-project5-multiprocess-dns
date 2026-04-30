sizes = [100, 250, 500, 750, 1000, 1500, 2000]

with open("large.txt", 'r') as large_file:
    for size in sizes:
        filename = str(size) + "names.txt"
        with open(filename, 'w', encoding="UTF-8") as newfile:
            i = 0
            while i < size:
                newfile.write(large_file.readline())
                i += 1
            
            
