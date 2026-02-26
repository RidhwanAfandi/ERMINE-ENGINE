using System;
using System.Runtime.CompilerServices;

namespace ErmineEngine
{
    /// <summary>
    /// Wrapper for UIBookCounterComponent - provides access to book collection tracking
    /// </summary>
    public class UIBookCounter
    {
        private ulong entityID;

        public UIBookCounter(ulong entityID)
        {
            this.entityID = entityID;
        }

        /// <summary>
        /// Get or set number of books collected
        /// </summary>
        public int BooksCollected
        {
            get => Internal_GetCollected(entityID);
            set => Internal_SetCollected(entityID, value);
        }

        /// <summary>
        /// Get total number of books in the level
        /// </summary>
        public int TotalBooks => Internal_GetTotal(entityID);

        /// <summary>
        /// Add one book to the counter (safe - won't exceed total)
        /// </summary>
        public void AddBook()
        {
            Internal_AddBook(entityID);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern int Internal_GetCollected(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetCollected(ulong entityID, int value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_AddBook(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern int Internal_GetTotal(ulong entityID);
    }
}
