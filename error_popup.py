import time

print("\n" + "="*100)
print(" " * 20 + "!!!   CRITICAL SYSTEM NOTICE   !!!")
print("="*100)
print()
print(" " * 10 + "THE SYSTEM WILL SHUT DOWN IN 10 SECONDS DUE TO A DETECTED FAILURE.")
print(" " * 10 + "A RESTART ATTEMPT WILL BE MADE AUTOMATICALLY 5 SECONDS AFTER SHUTDOWN.")
print()
print(" " * 10 + f"CHECK THE LOGS FOR MORE DETAILS.")
print()
print("="*100 + "\n")
    
time.sleep(10)  # Wait before killing the process